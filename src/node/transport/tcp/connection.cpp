#include "connection.hpp"
#include "transport/helpers/socket_addr.hpp"
#include "eventloop/eventloop.hpp"
#include "general/is_testnet.hpp"
#include "global/globals.hpp"
#include "uwebsockets/MoveOnlyFunction.h"
#include "version.hpp"
#include <chrono>

uint16_t TCPConnection::listen_port() const
{
    return conman.bindAddress.port;
}

std::optional<ConnectRequest> TCPConnection::connect_request() const
{
    return connectRequest;
}

TCPConnection::TCPConnection(Token, std::shared_ptr<uvw::tcp_handle> handle, const TCPConnectRequest& r, TCPConnectionManager& conman)
    : connectRequest(r)
    , conman(conman)
    , tcpHandle(std::move(handle))
{
}

std::shared_ptr<TCPConnection> TCPConnection::make_new(
    std::shared_ptr<uvw::tcp_handle> handle, const TCPConnectRequest& r, TCPConnectionManager& conman)
{
    return make_shared<TCPConnection>(Token {}, std::move(handle), r, conman);
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
    connection_log().info("a{}b closed: {} ({})",
        to_string(), errors::err_name(errcode), errors::strerror(errcode));
    on_close({
        .error = errcode,
    });
}

// CALLED BY OTHER THREAD
void TCPConnection::async_send(std::unique_ptr<char[]> data, size_t size)
{
    conman.async_call([this, data = std::move(data), size]() mutable {
        if (!tcpHandle->closing()) {
            if (tcpHandle->write(std::move(data), size)
                || tcpHandle->write_queue_size() > MAXBUFFER)
                close_internal(EBUFFERFULL);
        }
    });
}

TCPSockaddr TCPConnection::claimed_peer_addr() const
{
    if (inbound()) {
        // on inbound connection take the port the peer claims to listen on
        return { TCPSockaddr { peer_addr_native().ip, asserted_port() } };
    } else {
        // on outbound connection the port is the correct peer endpoint port
        return peer_addr_native();
    }
};
bool TCPConnection::inbound() const { 
    return connectRequest.inbound();
}
