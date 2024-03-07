#pragma once
#include "communication/buffers/recvbuffer.hpp"
#include "communication/buffers/sndbuffer.hpp"
#include "conman.hpp"
#include "connection_base.hpp"
#include "eventloop/types/conref_declaration.hpp"

class TCPConnection final : public ConnectionBase, public ConnectionBase::TCPData, public std::enable_shared_from_this<TCPConnection> {

    friend class UV_Helper;

    void async_send(std::unique_ptr<char[]> data, size_t size) override;
    ConnectionBase::Type type() const override { return ConnectionBase::Type::TCP; }
    uint16_t listen_port() override;

    struct Token {
    };

public:
    TCPConnection(Token, std::shared_ptr<uvw::tcp_handle> handle, const EndpointAddress& ea, bool inbound, UV_Helper& conman);
    [[nodiscard]] static std::shared_ptr<TCPConnection> make_new(
        std::shared_ptr<uvw::tcp_handle> handle, const EndpointAddress& peer, bool inbound, UV_Helper& conman);
    TCPConnection(const TCPConnection&) = delete;
    TCPConnection(TCPConnection&&) = delete;
    std::shared_ptr<ConnectionBase> get_shared() override
    {
        return shared_from_this();
    }

private:
    void start_read() override;
    void start_read_internal();

    void close(int errcode) override;
    void close_internal(int errcode);

private:
    UV_Helper& conman;
    std::shared_ptr<uvw::tcp_handle> tcpHandle;
};
