#pragma once
#include "../helpers/connect_request_base.hpp"
#include "../helpers/tcp_sockaddr.hpp"

struct ConnectRequest;
struct WSConnectRequest : public ConnectRequestBase {
    friend class ConnectionData;
    static WSConnectRequest make_inbound(WSSockaddr peer)
    {
        return { std::move(peer), -std::chrono::seconds(1) };
    }
    friend WSConnectRequest make_request(const WSSockaddr& connectTo, steady_duration sleptFor);

    const WSSockaddr address;
    operator ConnectRequest() const;

private:
    WSConnectRequest(WSSockaddr address, steady_duration sleptFor)
        : ConnectRequestBase(sleptFor)
        , address(std::move(address))
    {
    }
};

WSConnectRequest make_request(const WSSockaddr& connectTo, steady_duration sleptFor);
