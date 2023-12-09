#include "connection.hpp"
#include "eventloop/eventloop.hpp"
#include "global/globals.hpp"
#include "version.hpp"

static constexpr bool debug_refcount = true;
//////////////////////////////
// static members to be used as c callback functions in libuv
//////////////////////////////
void Connection::alloc_caller(uv_handle_t* handle, size_t suggested_size,
    uv_buf_t* buf)
{
    Connection& con = (*reinterpret_cast<Connection*>(handle->data));
    con.alloc_cb(suggested_size, buf);
}

void Connection::write_caller(uv_write_t* req, int status)
{
    Connection& con = (*reinterpret_cast<Connection*>(req->data));
    con.write_cb(status);
}

void Connection::read_caller(uv_stream_t* stream, ssize_t nread,
    const uv_buf_t* buf)
{
    Connection& con = (*reinterpret_cast<Connection*>(stream->data));
    con.read_cb(nread, buf);
}
void Connection::connect_caller(uv_connect_t* req, int status)
{
    Connection& con = (*reinterpret_cast<Connection*>(req->data));
    con.connect_cb(status);
    delete req;
    con.unref("connect");
}
void Connection::timeout_caller(uv_timer_t* handle)
{
    Connection& con = (*reinterpret_cast<Connection*>(handle->data));
    con.close(ETIMEOUT);
}
void Connection::close_caller(uv_handle_t* handle)
{
    Connection& con = (*reinterpret_cast<Connection*>(handle->data));
    handle->data = nullptr;
    con.unref("close");
}

//////////////////////////////
// members callbacks used for libuv
//////////////////////////////

void Connection::write_cb(int status)
{
    {
        std::unique_lock<std::mutex> lock(mutex);
        bufferedbytes -= buffers.front().buf.len;
        buffers.erase(buffers.begin());
    }
    if (state != State::CONNECTED && state != State::HANDSHAKE)
        return;
    if (status) {
        close(status);
        return;
    }
}
void Connection::read_cb(ssize_t nread, const uv_buf_t* /*buf*/)
{
    if (state != State::CONNECTED && state != State::HANDSHAKE)
        return;
    if (nread < 0) {
        close(nread);
        return;
    }
    if (nread == 0)
        return;
    if (handshakedata) {
        auto& hb = *handshakedata;
        hb.pos += nread;
        if (inbound) {
            if (hb.pos == hb.size(inbound)) {
                if (hb.waitForAck) {
                    assert(hb.pos == 25);
                    spdlog::debug("Handshake valid, peer version {}", peerVersion);
                    if (timer.data != nullptr)
                        uv_close((uv_handle_t*)&timer, close_caller);
                    handshakedata.reset(nullptr);
                    state = State::CONNECTED;
                    if (reconnectSleep) {
                        reconnectSleep = 0;
                    }
                    eventloop_notify();
                } else {
                    assert(hb.pos == 24);
                    assert(handshakedata->handshakesent == false);
                    peerVersion = hb.version(inbound);
                    if (peerVersion == 0) {
                        close(EHANDSHAKE);
                        return;
                    }
                    if (inbound) {
                        peerEndpointPort = hb.port(inbound);
                    }
                    if (!version_compatible(peerVersion)) {
                        close(EVERSION);
                        return;
                    }
                    send_handshake();
                    hb.waitForAck = true;
                }
            }
        } else {
            if (hb.pos == hb.size(inbound)) {
                peerVersion = hb.version(inbound);
                if (peerVersion == 0) {
                    close(EHANDSHAKE);
                    return;
                }
                if (!version_compatible(peerVersion)) {
                    close(EVERSION);
                    return;
                }
                spdlog::debug("Handshake valid, peer version {}", peerVersion);
                if (handshakedata->handshakesent == false)
                    send_handshake();
                if (timer.data != nullptr)
                    uv_close((uv_handle_t*)&timer, close_caller);
                handshakedata.reset(nullptr);
                state = State::CONNECTED;
                if (reconnectSleep) {
                    reconnectSleep = 0;
                }
                eventloop_notify();
                send_handshake_ack();
            }
        }
        return;
    }
    const size_t newpos = stagebuffer.pos + nread;
    stagebuffer.pos = newpos;
    if (newpos == 10) {
        if (int r = stagebuffer.allocate_body()) {
            close(r);
            return;
        }
    }
    if (stagebuffer.body.bytes.size() != 0 && stagebuffer.pos == 8 + stagebuffer.body.bytes.size()) {
        if (stagebuffer.finished()) {
            spdlog::debug("Received complete message");

            {
                std::unique_lock<std::mutex> lock(mutex);
                readbuffers.push_back(std::move(stagebuffer));
                stagebuffer.pos = 0;
            }
            eventloop_notify();
        } else {
            stagebuffer.realloc();
        }
    }
}
void Connection::alloc_cb(size_t /*suggested_size*/, uv_buf_t* buf)
{
    if (handshakedata) {
        buf->base = (char*)handshakedata->recvbuf.data() + handshakedata->pos;
        buf->len = handshakedata->size(inbound) - handshakedata->pos;
        return;
    }
    if (stagebuffer.pos < 10) {
        buf->base = (char*)stagebuffer.header + stagebuffer.pos;
        buf->len = 10 - stagebuffer.pos;
        return;
    }
    size_t offset = stagebuffer.pos - 8;
    buf->base = (char*)stagebuffer.body.bytes.data() + offset;
    buf->len = stagebuffer.body.bytes.size() - offset;
}
void Connection::connect_cb(int status)
{
    if (status < 0) {
        close(status);
        return;
    }
    state = State::HANDSHAKE;
    conman.perIpCounter.insert(peerAddress.ipv4);

    if ((status = start_read()))
        close(status);
    else
        send_handshake();
}

uint64_t Connection::idcounter = 1; // global counter of ids

Connection::Connection(Conman& conman, bool inbound, std::optional<uint32_t> reconnectSeconds)
    : reconnectSleep(reconnectSeconds)
    , inbound(inbound)
    , id(idcounter++)
    , connected_since(now_timestamp())
    , conman(conman)
    , handshakedata(new Handshakedata())
    , buffercursor(buffers.end())
{
    if (idcounter == 0)
        idcounter = 1; // id shall never be 0
    tcp.data = nullptr;
    timer.data = nullptr;
}
Connection::~Connection()
{
    // check if all handles are already closed.
    if (timer.data != nullptr || tcp.data != nullptr)
        spdlog::error("Memory leak: connection data!=nullptr");
}

void Connection::send_handshake()
{
    char* data = new char[24];
    memcpy(data, (inbound ? Handshakedata::accept_grunt : Handshakedata::connect_grunt), 14);
    uint32_t nver = hton32(version);
    memcpy(data + 14, &nver, 4);
    memset(data + 18, 0, 4);
    if (!inbound) {
        uint16_t portBe = hton16(conman.bindAddress.port);
        memcpy(data + 22, &portBe, 2);
        async_send(std::unique_ptr<char[]>(data), 24);
    } else {
        async_send(std::unique_ptr<char[]>(data), 22);
    }
    handshakedata->handshakesent = true;
}
void Connection::send_handshake_ack()
{
    char* data = new char[1];
    memcpy(data, "\0", 1);
    async_send(std::unique_ptr<char[]>(data), 1);
}

int Connection::send_buffers()
{
    std::unique_lock<std::mutex> lock(mutex);
    while (buffercursor != buffers.end()) {
        buffercursor->write_t.data = this;
        if (int r = uv_write(&buffercursor->write_t, (uv_stream_t*)&tcp,
                &buffercursor->buf, 1, write_caller))
            return r;
        ++buffercursor;
    }
    return 0;
}

int Connection::accept()
{
    int i;
    if ((i = uv_tcp_init(conman.server.loop, &tcp)))
        return i;
    if ((i = uv_accept((uv_stream_t*)&conman.server, (uv_stream_t*)&tcp)))
        return i;
    tcp.data = this;
    addref("tcp");

    // extract ip and port
    sockaddr_storage storage;
    int alen = sizeof(storage);
    if (i = uv_tcp_getpeername(&tcp, (struct sockaddr*)&storage, &alen); i != 0)
        return i;
    if (storage.ss_family != AF_INET)
        return EREFUSED;
    sockaddr_in* addr_i4 = (struct sockaddr_in*)&storage;
    peerAddress.ipv4 = IPv4(ntoh32(uint32_t(addr_i4->sin_addr.s_addr)));
    peerAddress.port = addr_i4->sin_port;
    spdlog::info("{} new incoming", to_string());
    if (!conman.count(peerAddress.ipv4)) {
        return EMAXCONNECTIONS;
    };
    state = State::HANDSHAKE;
    return 0;
}

int Connection::start_read()
{
    if (state != State::HANDSHAKE) {
        assert(state == State::CLOSING);
        return 0;
    }
    handshakedata.reset(new Handshakedata());
    assert(uv_timer_init(conman.server.loop, &timer) == 0);
    assert(uv_timer_start(&timer, timeout_caller, 5000, 0) == 0);
    timer.data = this;
    addref("timer");
    if (int i = uv_read_start((uv_stream_t*)&tcp, alloc_caller, read_caller))
        return i;
    return 0;
}

int Connection::connect(EndpointAddress a)
{
    int i;
    if ((i = uv_tcp_init(conman.server.loop, &tcp)))
        return i;
    tcp.data = this;
    addref("tcp");
    peerAddress = a;
    peerEndpointPort = a.port;
    uv_connect_t* p = new uv_connect_t;
    p->data = this;
    auto addr { a.sock_addr() };
    connection_log().info("{} connecting ", to_string());
    if (i = uv_tcp_connect(p, &tcp, (const sockaddr*)&addr, connect_caller); i != 0) {
        delete p;
    } else {
        state = State::CONNECTING;
        addref("connect");
    }
    return i;
}

void Connection::close(int errcode)
{
    if (state == State::CLOSING)
        return;
    if (state != State::CONNECTING) {
        conman.perIpCounter.erase(peerAddress.ipv4);
    }

    if (errors::is_malicious(errcode)) {
        if (reconnectSleep) {
            reconnectSleep = 60 * 60; // do not connect for 1 hour
        }
    }
    if (!inbound && (state == State::CONNECTING || state == State::HANDSHAKE)) {
        global().pel->async_report_failed_outbound(peerAddress);
    }

    state = State::CLOSING;
    connection_log().info("{} closed: {} ({})",
        to_string(), errors::err_name(errcode), errors::strerror(errcode));
    conman.peerServer.async_register_close(peerAddress.ipv4, errcode, logrow);
    if (eventloopref) {
        global().pel->async_erase(this);
    }
    std::unique_lock<std::mutex> lock(mutex);

    // delete unsent buffers
    while (buffercursor != buffers.end()) {
        bufferedbytes -= buffercursor->buf.len;
        buffers.erase(buffercursor++);
    }

    if (tcp.data != nullptr)
        uv_close((uv_handle_t*)&tcp, close_caller);
    if (timer.data != nullptr && uv_is_closing((uv_handle_t*)&timer) == 0) {
        uv_timer_stop(&timer);
        uv_close((uv_handle_t*)&timer, close_caller);
    }
    if (refcount == 0) {
        assert(timer.data == nullptr);
        conman.async_delete(this);
    }
}

void Connection::addref(const char* tag)
{
    std::unique_lock<std::mutex> lock(mutex);
    addref_locked(tag);
}
void Connection::addref_locked(const char* tag)
{
    refcount += 1;
    if (debug_refcount)
        spdlog::debug("                  {} [conn] addref -> {}, {}", id, refcount, tag);
}
//////////////////////////////
// BELOW METHODS CALLED FROM BOTH UV-THREAD AND MESSAGE PROCESSING THREAD
//////////////////////////////

// CALLED BY OTHER THREAD
std::vector<Rcvbuffer> Connection::extractMessages()
{
    std::unique_lock<std::mutex> lock(mutex);
    std::vector<Rcvbuffer> tmp;
    tmp.swap(readbuffers);
    return tmp;
}

std::string Connection::to_string() const
{
    return "(" + std::to_string(id) + ")" + (inbound ? "← " : "→ ") + peerAddress.to_string();
}

// POTENTIALLY CALLED BY OTHER THREAD
void Connection::eventloop_unref(const char* tag)
{
    int tmprefcount;
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (!eventloopref)
            return;
        eventloopref = false;
        refcount -= 1;
        if (debug_refcount) {
            spdlog::debug("                  {} [conn] unref -> {}, {}", id, refcount, tag);
        }
        tmprefcount = refcount;
    }
    if (tmprefcount == 0) {
        conman.async_delete(this);
    }
}

// POTENTIALLY CALLED BY OTHER THREAD
void Connection::unref(const char* tag)
{
    int tmprefcount;
    {
        std::unique_lock<std::mutex> lock(mutex);
        refcount -= 1;
        if (debug_refcount) {
            spdlog::debug("                  {} [conn] unref -> {}, {}", id, refcount, tag);
        }
        tmprefcount = refcount;
    }
    if (tmprefcount == 0) {
        conman.async_delete(this);
    }
}

// CALLED BY OTHER THREAD
void Connection::async_send(std::unique_ptr<char[]>&& data, size_t size)
{
    std::unique_lock<std::mutex> lock(mutex);
    buffers.emplace_back(std::move(data), size);
    bufferedbytes += size;
    if (buffercursor == buffers.end())
        --buffercursor;
    if (bufferedbytes >= MAXBUFFER) {
        async_close(EBUFFERFULL);
    }
    conman.async_send(this);
}

void Connection::asyncsend(Sndbuffer&& msg)
{
    msg.writeChecksum();
    async_send(std::move(msg.ptr), msg.fullsize());
}

void Connection::async_close(int32_t errcode) { conman.async_close(this, errcode); }

void Connection::eventloop_notify()
{
    bool ack = global().pel->async_process(this);
    std::unique_lock<std::mutex> lock(mutex);
    if (eventloopref == false && ack) {
        eventloopref = true;
        addref_locked("eventloop");
    }
}
