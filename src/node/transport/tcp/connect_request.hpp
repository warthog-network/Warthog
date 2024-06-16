#pragma once
#ifndef DISABLE_LIBUV
#include "../helpers/connect_request_base.hpp"
#include "../helpers/tcp_sockaddr.hpp"

struct TCPConnectRequest : public ConnectRequestBase {
    friend class ConnectionData;
    static TCPConnectRequest make_inbound(TCPSockaddr peer)
    {
        return { std::move(peer), -std::chrono::seconds(1) };
    }
    static TCPConnectRequest make_outbound(TCPSockaddr connectTo, steady_duration sleptFor){
        return {std::move(connectTo), sleptFor};
    }

    const TCPSockaddr address;
    void connect();

private:
    TCPConnectRequest(TCPSockaddr address, steady_duration sleptFor)
        : ConnectRequestBase(sleptFor)
        , address(std::move(address))
    {
    }
};

#endif
