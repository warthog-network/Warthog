#pragma once
#include "general/tcp_util.hpp"
#include "helpers/socket_addr.hpp"
#include <chrono>
#include <variant>

using steady_duration = std::chrono::steady_clock::duration;
struct ConnectRequestBase {
    const steady_duration sleptFor;
    auto inbound() const { return sleptFor.count() < 0; }
};

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

struct ConnectRequest {
    const Sockaddr address;
    const steady_duration sleptFor;
};
