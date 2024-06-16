#pragma once
#include "../helpers/connect_request_base.hpp"
#include "../helpers/tcp_sockaddr.hpp"

struct WebRTCConnectRequest : public ConnectRequestBase {
    friend class ConnectionData;
    static WebRTCConnectRequest make_inbound(WSSockaddr peer)
    {
        return { std::move(peer), -std::chrono::seconds(1) };
    }
    friend WebRTCConnectRequest make_request(const WSSockaddr& connectTo, steady_duration sleptFor);

    const WSSockaddr address;

private:
    WebRTCConnectRequest(WSSockaddr address, steady_duration sleptFor)
        : ConnectRequestBase(sleptFor)
        , address(std::move(address))
    {
    }
};

WebRTCConnectRequest make_request(const WSSockaddr& connectTo, steady_duration sleptFor);
