#pragma once

#include "../connect_request.hpp"
#include "transport/connection_base.hpp"

class WSSession;
class WSConnectionManager;

class WSConnection final : public ConnectionBase, public std::enable_shared_from_this<WSConnection> {
    void async_send(std::unique_ptr<char[]> data, size_t size) override;
    uint16_t listen_port() const override;
    ConnectRequest connect_request() const override;
    struct CreationToken {};

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
    std::weak_ptr<ConnectionBase> get_weak() override
    {
        return weak_from_this();
    }

    Sockaddr connection_peer_addr() const override { return { connection_peer_addr_native() }; }
    WSSockaddr connection_peer_addr_native() const { return connectRequest.address; }
    Sockaddr claimed_peer_addr() const override;
    bool inbound() const override;

private:
    void start_read() override;
    void close(int errcode) override;

    std::weak_ptr<WSSession> session;
    const WSConnectRequest connectRequest;
    WSConnectionManager& conman;
};
