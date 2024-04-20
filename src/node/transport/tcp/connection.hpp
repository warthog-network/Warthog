#pragma once
#include "../connection_base.hpp"
#include "communication/buffers/recvbuffer.hpp"
#include "communication/buffers/sndbuffer.hpp"
#include "conman.hpp"
#include "eventloop/types/conref_declaration.hpp"

class TCPConnection final : public ConnectionBase, public ConnectionBase::TCPData, public std::enable_shared_from_this<TCPConnection> {

    friend class TCPConnectionManager;

    void async_send(std::unique_ptr<char[]> data, size_t size) override;
    ConnectionBase::Type type() const override { return ConnectionBase::Type::TCP; }
    uint16_t listen_port() const override;
    ConnectRequest connect_request() const override;

    struct Token {
    };

public:
    TCPConnection(Token, std::shared_ptr<uvw::tcp_handle> handle, const TCPConnectRequest& r, TCPConnectionManager& conman);
    [[nodiscard]] static std::shared_ptr<TCPConnection> make_new(
        std::shared_ptr<uvw::tcp_handle> handle, const TCPConnectRequest& r, TCPConnectionManager& conman);
    TCPConnection(const TCPConnection&) = delete;
    TCPConnection(TCPConnection&&) = delete;
    std::shared_ptr<ConnectionBase> get_shared() override
    {
        return shared_from_this();
    }
    Sockaddr claimed_peer_addr() const override;
    Sockaddr connection_peer_addr() const override { return { connection_peer_addr_native() }; }
    TCPSockaddr connection_peer_addr_native() const { return connectRequest.address; }
    bool inbound() const override;

private:
    void start_read() override;
    void start_read_internal();

    void close(int errcode) override;
    void close_internal(int errcode);

private:
    const TCPConnectRequest connectRequest;
    TCPConnectionManager& conman;
    std::shared_ptr<uvw::tcp_handle> tcpHandle;
};
