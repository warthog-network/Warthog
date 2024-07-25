#pragma once
#include "transport/helpers/connect_request_base.hpp"
#include "transport/helpers/tcp_sockaddr.hpp"

struct WSConnectRequest : public ConnectRequestBase {
    friend class ConnectionData;
    static WSConnectRequest make_inbound(WSPeeraddr peer)
    {
        return { std::move(peer), -std::chrono::seconds(1) };
    }

    const WSPeeraddr address;

private:
    WSConnectRequest(WSPeeraddr address, steady_duration sleptFor)
        : ConnectRequestBase(sleptFor)
        , address(std::move(address))
    {
    }
};
