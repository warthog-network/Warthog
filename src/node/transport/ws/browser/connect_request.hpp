#pragma once
#include "transport/helpers/connect_request_base.hpp"
#include "transport/helpers/tcp_sockaddr.hpp"
#include "transport/ws/browser/ws_urladdr.hpp"

struct WSBrowserConnectRequest : public ConnectRequestBase {
    friend class ConnectionData;
    [[nodiscard]] static WSBrowserConnectRequest make_outbound(WSUrladdr peer, steady_duration sleptFor = std::chrono::seconds(0))
    {
        return { std::move(peer), sleptFor };
    }
    void connect() const;
    auto& address() const { return _address; }

private:
    WSUrladdr _address;
    WSBrowserConnectRequest(WSUrladdr address, steady_duration sleptFor)
        : ConnectRequestBase(sleptFor)
        , _address(std::move(address))
    {
    }
};
