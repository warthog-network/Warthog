#include "conman.hpp"
#include "connection.hpp"
#include "eventloop/eventloop.hpp"
#include "global/globals.hpp"
#include "uvw.hpp"
#include <memory>
namespace {
[[nodiscard]] std::optional<TCPSockaddr> get_ipv4_endpoint(const uvw::tcp_handle& handle)
{
    sockaddr_storage storage;
    int alen = sizeof(storage);
    assert(uv_tcp_getpeername(handle.raw(), (struct sockaddr*)&storage, &alen) == 0);
    if (storage.ss_family != AF_INET)
        return {};
    sockaddr_in* addr_i4 = (struct sockaddr_in*)&storage;
    return TCPSockaddr(IPv4(ntoh32(uint32_t(addr_i4->sin_addr.s_addr))), addr_i4->sin_port);
}
}

TCPConnection& TCPConnectionManager::insert_connection(std::shared_ptr<uvw::tcp_handle>& tcpHandle, const TCPConnectRequest& r)
{
    auto con { TCPConnection::make_new(tcpHandle, r, *this) };
    auto iter { tcpConnections.insert(con).first };
    tcpHandle->data(con);
    tcpHandle->on<uvw::close_event>([this, iter](const uvw::close_event&, uvw::tcp_handle& client) {
        client.data(nullptr);
        tcpConnections.erase(iter);
    });
    tcpHandle->on<uvw::end_event>([](const uvw::end_event&, uvw::tcp_handle& client) {
        client.data<TCPConnection>()->close_internal(UV_EOF);
    });
    tcpHandle->on<uvw::error_event>([](const uvw::error_event& e, uvw::tcp_handle& client) {
        client.data<TCPConnection>()->close_internal(e.code());
    });
    tcpHandle->on<uvw::shutdown_event>([](const uvw::shutdown_event&, uvw::tcp_handle& client) { client.close(); });
    tcpHandle->on<uvw::data_event>([](const uvw::data_event& de, uvw::tcp_handle& client) {
        client.data<TCPConnection>()->on_message({ reinterpret_cast<uint8_t*>(de.data.get()), de.length });
    });
    return *con;
};

TCPConnectionManager::TCPConnectionManager(std::shared_ptr<uvw::loop> loop, PeerServer& ps, const ConfigParams& cfg)
    : bindAddress(cfg.node.bind)
{
    listener = loop->resource<uvw::tcp_handle>();
    assert(listener);
    listener->on<uvw::error_event>([](const uvw::error_event&, uvw::tcp_handle&) {
        spdlog::error("");
    });
    listener->on<uvw::listen_event>([this, &ps](const uvw::listen_event&, uvw::tcp_handle& server) {
        if (config().node.isolated)
            return;
        std::shared_ptr<uvw::tcp_handle> tcpHandle = server.parent().resource<uvw::tcp_handle>();
        assert(server.accept(*tcpHandle) == 0);
        auto endpoint { get_ipv4_endpoint(*tcpHandle) };
        if (endpoint) {
            auto connectRequest { TCPConnectRequest::make_inbound(*endpoint) };
            auto connection { insert_connection(tcpHandle, connectRequest).shared_from_this() };
            ps.authenticate_inbound(endpoint.value().ip(), TransportType::TCP, connection);
        }
    });
    wakeup = loop->resource<uvw::async_handle>();
    assert(wakeup);
    wakeup->on<uvw::async_event>([&](uvw::async_event&, uvw::async_handle&) {
        on_wakeup();
    });

    spdlog::info("P2P endpoint is {}.", bindAddress.to_string());
    int i = 0;
    if ((i = listener->bind(bindAddress)) || (i = listener->listen()))
        throw std::runtime_error(
            "Cannot start connection manager: " + std::string(errors::err_name(i)));
    assert(0 == listener->listen());
}
void TCPConnectionManager::on_wakeup()
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

void TCPConnectionManager::handle_event(GetPeers&& e)
{
    std::vector<APIPeerdata> data;
    for (auto c : tcpConnections) {
        data.push_back({ c->peer_addr_native(), c->created_at_timestmap() });
    }
    e.cb(std::move(data));
}

void TCPConnectionManager::handle_event(Connect&& c)
{
    TCPConnectRequest& r { c };
    connection_log().info("{} connecting ", r.address().to_string()); // TODO: do connection_log
    auto& loop { listener->parent() };
    auto tcp { loop.resource<uvw::tcp_handle>() };
    auto err { tcp->connect(r.address().sock_addr()) };
    if (err) {
        global().core->on_failed_connect(r, Error(err));
        return;
    }
    auto& connection { insert_connection(tcp, r) };
    global().peerServer->log_outbound(c.address().ip(), connection.shared_from_this());
    connection.start_read();
}

void TCPConnectionManager::handle_event(Inspect&& e)
{
    e.callback(*this);
}

void TCPConnectionManager::handle_event(DeferFunc&& f)
{
    f.callback();
};

void TCPConnectionManager::shutdown(int32_t reason)
{
    if (closing == true)
        return;
    closing = true;
    wakeup->close();
    listener->close();
    for (auto& c : tcpConnections)
        c->close_internal(reason);
}
