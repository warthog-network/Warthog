#include "connection.hpp"
#include "eventloop/eventloop.hpp"
#include "general/is_testnet.hpp"
#include "global/globals.hpp"
#include "version.hpp"
#include <chrono>

uint16_t TCPConnection::listen_port() {
    return conman.bindAddress.port;
};

TCPConnection::TCPConnection(Token, std::shared_ptr<uvw::tcp_handle> handle, const EndpointAddress& peer, bool inbound, UV_Helper& conman)
    : ConnectionBase(peer, inbound)
    , conman(conman)
    , tcpHandle(std::move(handle))
{
}

std::shared_ptr<TCPConnection> TCPConnection::make_new(
    std::shared_ptr<uvw::tcp_handle> handle, const EndpointAddress& peer, bool inbound, UV_Helper& conman)
{
    return make_shared<TCPConnection>(Token {}, std::move(handle), peer, inbound, conman);
}

void TCPConnection::start_read()
{
    conman.async_call([c = shared_from_this()]() {
        c->start_read_internal();
    });
};
void TCPConnection::start_read_internal()
{
    if (tcpHandle->closing())
        return;
    if (int e = tcpHandle->read(); e)
        close_internal(e);
    else
        on_connected();
}

void TCPConnection::close(int errcode)
{
    conman.async_call([errcode, c = shared_from_this()]() {
        c->close_internal(errcode);
    });
}

void TCPConnection::close_internal(int errcode)
{
    if (tcpHandle->closing())
        return;
    tcpHandle->close();
    connection_log().info("{} closed: {} ({})",
        to_string(), errors::err_name(errcode), errors::strerror(errcode));
    on_close({
        .error = errcode,
    });
}

// CALLED BY OTHER THREAD
void TCPConnection::async_send(std::unique_ptr<char[]> data, size_t size)
{
    // workaround with Wrapper:
    // std::function needs to be copyable and does not support capturing std::unique_ptr
    // should we switch to C++23 and use std::move_only_function?
    struct Wrapper {
        std::unique_ptr<char[]> data; 
        size_t size;
    };
    
    conman.async_call([this, wrapper = std::make_shared<Wrapper>(std::move(data),size)]() mutable {
            auto &data=wrapper->data;
            auto size=wrapper->size;
        if (!tcpHandle->closing()) {
            if (tcpHandle->write(std::move(data), size)
                || tcpHandle->write_queue_size() > MAXBUFFER)
                close_internal(EBUFFERFULL);
        }
    });
}
