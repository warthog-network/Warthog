#include "connection.hpp"
bool WSConnection::connect(const WSBrowserConnectRequest& r)
{
    auto emsCon { EmscriptenWSConnection::make_new(r.address.url) };
    if (!emsCon.has_value())
        return false;

    auto p { std::make_shared<WSConnection>(CreationToken {}, r, std::move(*emsCon)) };
    p->self = std::move(p);

    return true;
}
WSConnection::WSConnection(CreationToken, const WSBrowserConnectRequest& r, EmscriptenWSConnection&& emscon)
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

std::optional<ConnectRequest> WSConnection::connect_request() const
{
    return ConnectRequest{connectRequest};
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

