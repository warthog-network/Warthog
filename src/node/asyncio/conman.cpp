#include "conman.hpp"
#include "connection.hpp"
#include "eventloop/eventloop.hpp"
#include "global/globals.hpp"
static constexpr bool debug_refcount = false;

//////////////////////////////
// Callers (static libuv callback functions)
void Conman::new_connection_caller(uv_stream_t* server, int status)
{
    Conman& c = (*reinterpret_cast<Conman*>(server->data));
    if (!c.closing) {
        c.on_connect(status);
    }
}
void Conman::wakeup_caller(uv_async_t* handle)
{
    Conman& cm = (*reinterpret_cast<Conman*>(handle->data));
    cm.on_wakeup();
}
void Conman::close_caller(uv_handle_t* handle)
{
    Conman& cm = (*reinterpret_cast<Conman*>(handle->data));
    handle->data = nullptr;
    cm.unref("closed");
}
void Conman::reconnect_caller(uv_timer_t* handle)
{
    ReconnectTimer& timer = (*reinterpret_cast<ReconnectTimer*>(handle->data));
    timer.conman->on_reconnect_wakeup(timer);
}
void Conman::reconnect_closed_cb(uv_handle_t* handle)
{
    ReconnectTimer& timer = (*reinterpret_cast<ReconnectTimer*>(handle->data));
    timer.conman->on_reconnect_closed(timer);
}

// ip counting
bool Conman::count(IPv4 ip)
{
    return perIpCounter.insert(ip, max_conn_per_ip);
}
void Conman::count_force(IPv4 ip)
{
    perIpCounter.insert(ip);
}
void Conman::uncount(IPv4 ip)
{
    perIpCounter.erase(ip);
}

// reference counting
void Conman::unlink(Connection* const pcon)
{
    connections.erase(pcon);
    unref("connection");
}
void Conman::addref(const char* tag)
{
    std::unique_lock<std::mutex> lock(mutex);
    refcount += 1;
    if (debug_refcount)
        spdlog::debug("[conman] addref -> {}, {}", refcount, tag);
}
void Conman::unref(const char* tag)
{
    refcount -= 1;
    if (debug_refcount)
        spdlog::debug("[conman] unref -> {}, {}", refcount, tag);
    if (closing) {
        if (refcount == 1) { // 1 for the wakeup callback
            uv_close((uv_handle_t*)&wakeup, close_caller);
        }
    }
}

void Conman::async_send(Connection* pcon) // CALLED BY PROCESSING THREAD
{
    std::unique_lock<std::mutex> lock(mutex);
    events.push(Send { pcon });
    uv_async_send(&wakeup);
}
void Conman::async_delete(Connection* pcon) // POTENTIALLY CALLED BY OTHER THREAD
{
    std::unique_lock<std::mutex> lock(mutex);
    events.push(Delete { pcon });
    uv_async_send(&wakeup);
}
void Conman::async_close(Connection* pcon,
    int32_t error) // POTENTIALLY CALLED BY OTHER THREAD
{
    std::unique_lock<std::mutex> lock(mutex);
    events.push(Close { pcon, error });
    uv_async_send(&wakeup);
}
void Conman::async_validate(Connection* c, bool accept, int64_t rowid)
{
    std::unique_lock<std::mutex> lock(mutex);
    c->addref("validate");
    events.push(Validation { c, accept, rowid });
    uv_async_send(&wakeup);
}

Conman::Conman(uv_loop_t* l, PeerServer& peerServer, const Config& config,
    int backlog)
    : peerServer(peerServer)
    , bindAddress(config.node.bind)
{
    int i;
    server.data = wakeup.data = nullptr;
    if ((i = uv_tcp_init(l, &server)))
        throw std::runtime_error("Cannot initialize TCP Server");
    server.data = this;
    addref("TCP Server");
    // only accept IPv4
    auto addr { bindAddress.sock_addr() };
    spdlog::info("P2P endpoint is {}.", bindAddress.to_string());
    if ((i = uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0)))
        goto error;
    if ((i = uv_listen((uv_stream_t*)&server, backlog,
             new_connection_caller)))
        goto error;
    if ((i = uv_async_init(l, &wakeup, wakeup_caller) < 0))
        goto error;
    wakeup.data = this;
    addref("wakeup");

    return;
error:
    throw std::runtime_error(
        "Cannot start connection manager (memory will leak): " + std::string(errors::err_name(i)));
}
void Conman::on_connect(int status)
{
    if (status != 0) {
        spdlog::error("Failed to accept connection: {}, status:{}",
            std::string(errors::err_name(status)), status);
        return;
    }
    auto p = connections.emplace(new Connection(*this, true));
    auto& conn = **p.first;
    addref("connection");
    if (status = conn.accept(); status != 0)
        return conn.close(status);
    peerServer.async_validate(*this, &conn);
}
void Conman::on_wakeup()
{
    decltype(events) tmp;
    { // lock for very short time (swap)
        std::unique_lock<std::mutex> lock(mutex);
        std::swap(tmp, events);
    }

    while (!tmp.empty()) {
        std::visit([&](auto& e) { handle_event(std::move(e)); }, tmp.front());
        tmp.pop();
    }
}
void Conman::on_reconnect_wakeup(ReconnectTimer& t)
{
    uv_close((uv_handle_t*)&t.uv_timer, &Conman::reconnect_closed_cb);
}
void Conman::on_reconnect_closed(ReconnectTimer& t)
{
    if (t.nextReconnectSleep > 0) {
        connect(t.address, t.nextReconnectSleep);
    }
    reconnectTimers.erase(t.iter);
    unref("reconnect closed");
}

void Conman::handle_event(Delete&& e)
{
    assert(e.c->state == Connection::State::CLOSING);
    assert(e.c->timer.data == nullptr);
    unlink(e.c);
    if (e.c->reconnectSleep.has_value() && !e.c->inbound) {
        auto a = e.c->peerAddress;
        const size_t seconds = e.c->reconnectSleep.value();
        const size_t milliseconds = seconds * 1000;
        reconnectTimers.push_back(
            ReconnectTimer {
                .conman = this,
                .uv_timer {},
                .address { a },
                .nextReconnectSleep = std::max(seconds, std::min(2 * seconds + 1, 60ul)),
                .iter {} });
        auto iter = std::prev(reconnectTimers.end());
        auto& timer = *iter;
        addref("timer");
        timer.uv_timer.data = &timer;
        timer.iter = iter;
        assert(uv_timer_init(loop(), &timer.uv_timer) == 0);
        uv_timer_start(&timer.uv_timer, &Conman::reconnect_caller, milliseconds, 0);
    }
    delete e.c;
}
void Conman::handle_event(Close&& e)
{
    e.c->close(e.reason);
}
void Conman::handle_event(Send&& e)
{
    e.c->send_buffers();
}

void Conman::handle_event(Validation&& e)
{
    Connection* c = e.c;
    c->unref("validate");
    c->logrow = e.rowid;
    if (e.accept) { // accept
        c->start_read();
    } else {
        c->close(EREFUSED);
    }
}

void Conman::handle_event(GetPeers&& e)
{
    std::vector<APIPeerdata> data;
    for (Connection* c : connections) {
        APIPeerdata item;
        item.address = c->peerAddress;
        item.since = c->connected_since;
        data.push_back(item);
    }
    e.cb(std::move(data));
}

void Conman::handle_event(Connect&& c)
{
    if (!closing) {
        connect(c.a, c.reconnectSleep);
    }
}

void Conman::handle_event(Inspect&& e)
{
    e.callback(*this);
}

void Conman::close(int32_t reason)
{
    if (closing == true)
        return;
    closing = true;
    global().pel->async_shutdown(reason);
    // uv_async_send(&wakeup);
    if (server.data != nullptr) {
        uv_close((uv_handle_t*)&server, close_caller);
    }
    for (auto& c : connections) {
        c->reconnectSleep.reset(); // avoid reconnect
        c->close(reason);
    }
    for (auto& t : reconnectTimers) {
        t.nextReconnectSleep = 0;
        uv_timer_stop(&t.uv_timer);
        uv_close((uv_handle_t*)&t.uv_timer, reconnect_closed_cb);
    }
    peerServer.async_shutdown();
    if (closing && refcount == 1) { // 1 for the wakeup callback
        uv_close((uv_handle_t*)&wakeup, close_caller);
    }
}

void Conman::connect(EndpointAddress a, std::optional<uint32_t> reconnectSleep)
{
    auto p = connections.emplace(new Connection(*this, false, reconnectSleep));
    auto& conn = **p.first;
    addref("connection");
    if (int i = conn.connect(a)) {
        conn.close(i);
        spdlog::error("Cannot connect: {}", errors::err_name(i));
    }
}
