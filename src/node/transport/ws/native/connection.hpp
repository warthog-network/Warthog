#pragma once

#ifndef DISABLE_LIBUV
#include "connect_request.hpp"
#include "transport/connection_base.hpp"

class WSSession;
class WSConnectionManager;

class WSConnection final : public AuthenticatableConnection, public std::enable_shared_from_this<WSConnection> {
    void async_send(std::unique_ptr<char[]> data, size_t size) override;
    uint16_t listen_port() const override;
    std::optional<ConnectRequest> connect_request() const override { return {}; }
    struct CreationToken { };

    friend class WSSession;

public:
    WSConnection(CreationToken, std::weak_ptr<WSSession> handle, const WSConnectRequest& r, WSConnectionManager& conman);

    [[nodiscard]] static std::shared_ptr<WSConnection> make_new(
        std::weak_ptr<WSSession> handle, const WSConnectRequest& r, WSConnectionManager& conman);
    WSConnection(const WSConnection&) = delete;
    WSConnection(WSConnection&&) = delete;
    std::shared_ptr<ConnectionBase> get_shared() override
    {
        return shared_from_this();
    }
    virtual ConnectionVariant get_shared_variant() override
    {
        return shared_from_this();
    }
    std::weak_ptr<ConnectionBase> get_weak() override
    {
        return weak_from_this();
    }

    Peeraddr peer_addr() const override { return { connection_peer_addr_native() }; }
    WSPeeraddr connection_peer_addr_native() const { return connectRequest.address; }
    bool inbound() const override;

private:
    void start_read() override;
    void close(int errcode) override;

    std::weak_ptr<WSSession> session;
    const WSConnectRequest connectRequest;
    WSConnectionManager& conman;
};
#endif
