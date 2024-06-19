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

    void connect();
    auto& address() const { return _address;}

private:
    TCPSockaddr _address;
    TCPConnectRequest(TCPSockaddr address, steady_duration sleptFor)
        : ConnectRequestBase(sleptFor)
        , _address(std::move(address))
    {
    }
};

#endif
