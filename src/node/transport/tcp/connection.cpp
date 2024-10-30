#include "connection.hpp"
#include "eventloop/eventloop.hpp"
#include "general/is_testnet.hpp"
#include "global/globals.hpp"
#include "transport/helpers/peer_addr.hpp"
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

void TCPConnection::close(Error e)
{
    conman.async_call([e, c = shared_from_this()]() {
        c->close_internal(e);
    });
}

void TCPConnection::close_internal(Error e)
{
    if (tcpHandle->closing())
        return;
    tcpHandle->close();
    connection_log().info("{} closed: {}", tag_string(), e.format());
    on_close(e);
}

// CALLED BY OTHER THREAD
void TCPConnection::async_send(std::unique_ptr<char[]> data, size_t size)
{
    conman.async_call([w = weak_from_this(), data = std::move(data), size]() mutable {
        if (auto c { w.lock() }; c && !c->tcpHandle->closing()) {
            if (c->tcpHandle->write(std::move(data), size)
                || c->tcpHandle->write_queue_size() > MAXBUFFER)
                c->close_internal(EBUFFERFULL);
        }
    });
}

TCPPeeraddr TCPConnection::claimed_peer_addr() const
{
    if (inbound()) {
        // on inbound connection take the port the peer claims to listen on
        return { TCPPeeraddr { peer_addr_native().ip, asserted_port() } };
    } else {
        // on outbound connection the port is the correct peer endpoint port
        return peer_addr_native();
    }
};
bool TCPConnection::inbound() const
{
    return connectRequest.inbound();
}
