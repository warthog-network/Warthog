#pragma once
#ifndef DISABLE_LIBUV
#include "../helpers/connect_request_base.hpp"
#include "../helpers/tcp_sockaddr.hpp"

struct TCPConnectRequest : public ConnectRequestBase {
    friend class ConnectionData;
    static TCPConnectRequest make_inbound(TCPPeeraddr peer)
    {
        return { std::move(peer), -std::chrono::seconds(1) };
    }
    static TCPConnectRequest make_outbound(TCPPeeraddr connectTo, steady_duration sleptFor){
        return {std::move(connectTo), sleptFor};
    }

    void connect();
    auto& address() const { return _address;}

private:
    TCPPeeraddr _address;
    TCPConnectRequest(TCPPeeraddr address, steady_duration sleptFor)
        : ConnectRequestBase(sleptFor)
        , _address(std::move(address))
    {
    }
};

#endif
