#pragma once
#include "../helpers/connect_request_base.hpp"
#include "../helpers/tcp_sockaddr.hpp"

struct ConnectRequest;
struct TCPConnectRequest : public ConnectRequestBase {
    friend class ConnectionData;
    static TCPConnectRequest inbound(TCPSockaddr peer)
    {
        return { std::move(peer), -std::chrono::seconds(1) };
    }
    friend TCPConnectRequest make_request(const TCPSockaddr& connectTo, steady_duration sleptFor);

    const TCPSockaddr address;
    operator ConnectRequest() const;

private:
    TCPConnectRequest(TCPSockaddr address, steady_duration sleptFor)
        : ConnectRequestBase(sleptFor)
        , address(std::move(address))
    {
    }
};

TCPConnectRequest make_request(const TCPSockaddr& connectTo, steady_duration sleptFor);
