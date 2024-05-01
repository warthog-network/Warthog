#include "connection.hpp"
std::shared_ptr<WSConnection> WSConnection::make_new(const WSConnectRequest& r)
{
    std::string url { r.address.to_string() + "/" };
    auto emsCon { EmscriptenWSConnection::make_new(url) };
    if (!emsCon.has_value())
        return {};

    auto p { std::make_shared<WSConnection>(CreationToken {}, r, std::move(*emsCon)) };
    p->self = p;

    return p;
}
WSConnection::WSConnection(CreationToken, const WSConnectRequest& r, EmscriptenWSConnection&& emscon)
    : connectRequest(r)
    , emscriptenConnection(std::move(emscon))
{
    emscriptenConnection.set_callbacks(
        { .open_callback {
              [&]() {
                  return on_open();
              } },
            .close_callback {
                [&](EmscriptenWSConnection::CloseInfo e) {
                    return on_close(e);
                } },
            .error_callback {
                [&]() {
                    return on_error();
                } },
            .message_callback {
                [&](const EmscriptenWSConnection::Message& m) {
                    return on_message(m);
                } } });
}

void WSConnection::async_send(std::unique_ptr<char[]> data, size_t size)
{
    emscriptenConnection.send_binary({ (uint8_t*)data.get(), size });
}

uint16_t WSConnection::listen_port() const
{
    return 0;
}

ConnectRequest WSConnection::connect_request() const
{
    return connectRequest;
}

Sockaddr WSConnection::claimed_peer_addr() const
{
    if (inbound()) {
        // on inbound connection take the port the peer claims to listen on
        return { WSSockaddr { connection_peer_addr_native().ipv4, asserted_port() } };
    } else {
        // on outbound connection the port is the correct peer endpoint port
        return connection_peer_addr();
    }
}

bool WSConnection::inbound() const
{
    return connectRequest.inbound();
}

int WSConnection::on_open()
{
    ConnectionBase::on_connected();
    return 0;
}

int WSConnection::on_error()
{
    return 0;
}

void WSConnection::notify_closed(int32_t reason)
{
    if (!closed) {
        closed = true;
        ConnectionBase::on_close({ .error = reason });
    }
}

void WSConnection::close(int reason)
{
    {
        std::lock_guard l(close_mutex);
        notify_closed(reason);
    }
    emscriptenConnection.close();
}

int WSConnection::on_close(EmscriptenWSConnection::CloseInfo)
{
    std::lock_guard l(close_mutex);
    notify_closed(EWEBSOCK);
    self.reset(); // destroy self shared_ptr to avoid memory leak
    return 0;
}

int WSConnection::on_message(EmscriptenWSConnection::Message msg)
{
    if (msg.isText)
        return -1;
    ConnectionBase::on_message(msg.bytes);
    return 0;
}

void WSConnection::start_read()
{
}
