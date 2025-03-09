#pragma once
#include "../connection_base.hpp"
#include "communication/buffers/recvbuffer.hpp"
#include "communication/buffers/sndbuffer.hpp"
#include "conman.hpp"
#include "eventloop/types/conref_declaration.hpp"

class TCPConnection final : public AuthenticatableConnection, public std::enable_shared_from_this<TCPConnection> {

    friend class TCPConnectionManager;

    void send_impl(std::unique_ptr<char[]> data, size_t size) override;
    uint16_t listen_port() const override;
    std::optional<ConnectRequest> connect_request() const override;

    struct Token {
    };

public:
    TCPConnection(Token, std::shared_ptr<uvw::tcp_handle> handle, const TCPConnectRequest& r, TCPConnectionManager& conman);
    [[nodiscard]] static std::shared_ptr<TCPConnection> make_new(
        std::shared_ptr<uvw::tcp_handle> handle, const TCPConnectRequest& r, TCPConnectionManager& conman);
    TCPConnection(const TCPConnection&) = delete;
    TCPConnection(TCPConnection&&) = delete;
    virtual bool is_tcp() const override { return true; }
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

    TCPPeeraddr claimed_peer_addr() const;
    Peeraddr peer_addr() const override { return { peer_addr_native() }; }
    TCPPeeraddr peer_addr_native() const { return connectRequest.address(); }
    bool inbound() const override;

private:
    void start_read() override;
    void start_read_internal();

    void close(Error) override;
    void close_internal(Error);

private:
    const TCPConnectRequest connectRequest;
    TCPConnectionManager& conman;
    std::shared_ptr<uvw::tcp_handle> tcpHandle;
};
