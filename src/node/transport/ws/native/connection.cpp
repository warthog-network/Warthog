#include "connection.hpp"
#include "transport/ws/native/conman.hpp"

WSConnection::WSConnection(CreationToken, std::weak_ptr<WSSession> handle, const WSConnectRequest& r, WSConnectionManager& conman)
    : session(handle)
    , connectRequest(r)
    , conman(conman)
{
}

std::shared_ptr<WSConnection> WSConnection::make_new(
    std::weak_ptr<WSSession> handle, const WSConnectRequest& r, WSConnectionManager& conman)
{
    return std::make_shared<WSConnection>(CreationToken {}, std::move(handle), r, conman);
}

void WSConnection::async_send(std::unique_ptr<char[]> data, size_t size)
{
    conman.async_send(session, std::move(data), size);
}

uint16_t WSConnection::listen_port() const
{
    return conman.startOptions.port;
}

ConnectRequest WSConnection::connect_request() const
{
    return connectRequest;
}

Sockaddr WSConnection::claimed_peer_addr() const{
    if (inbound()) {
        // on inbound connection take the port the peer claims to listen on
        return { WSSockaddr { connection_peer_addr_native().ipv4, asserted_port() } };
    } else {
        // on outbound connection the port is the correct peer endpoint port
        return connection_peer_addr();
    }
}

bool WSConnection::inbound() const{
    return connectRequest.inbound();
}

void WSConnection::start_read()
{
    conman.start_read(session);
}

void WSConnection::close(int errcode)
{
    conman.close(session, errcode);
}
