#pragma once

#include "communication/buffers/recvbuffer.hpp"
#include "communication/version.hpp"
#include "eventloop/timer_element.hpp"
#include "eventloop/types/conref_declaration.hpp"
#include "peerserver/connection_data.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>
class ConnectionBase;
class TCPConnection;
class WSConnection;
class RTCConnection;
namespace uvw {
class timer_handle;
}
struct Sockaddr;

struct HandshakeState {

    std::array<uint8_t, 24> recvbuf; // 14 bytes for "WARTHOG GRUNT!" and 4
                                     // bytes for version + 4 extra bytes
                                     // (in case of outbound: + 2 bytes for
                                     //  sending port port)
    TimerElement expire_timer;

    static constexpr const char connect_grunt[] = "WARTHOG GRUNT?";
    static constexpr const char accept_grunt[] = "WARTHOG GRUNT!";
    static constexpr const char connect_grunt_testnet[] = "TESTNET GRUNT?";
    static constexpr const char accept_grunt_testnet[] = "TESTNET GRUNT!";
    auto data() { return recvbuf.data(); }
    size_t size(bool inbound) { return (inbound ? 24 : 22); }
    size_t remaining(bool inbound) { return size(inbound) - pos; }
    uint8_t pos = 0;
    bool handshakesent = false;
    struct Parsed {
        ProtocolVersion version;
        std::optional<uint16_t> port;
    };
    Parsed parse(bool inboound);
};

struct AckState {
    AckState(HandshakeState::Parsed p)
        : handshakeData(std::move(p))
    {
    }
    HandshakeState::Parsed handshakeData;
};

struct MessageState : public AckState {
    MessageState(AckState s)
        : AckState(s)
    {
    }

    Rcvbuffer currentBuffer;
    std::vector<Rcvbuffer> readbuffers;
};

class ConnectionBase : public peerserver::Connection {
public:
    using variant_t = std::variant<
#ifndef DISABLE_LIBUV
        std::shared_ptr<TCPConnection>,
#endif
        std::shared_ptr<WSConnection>,
        std::shared_ptr<RTCConnection>>;
    struct ConnectionVariant : public variant_t {
        using variant_t::variant;
        [[nodiscard]] ConnectionBase* base();
        [[nodiscard]] const ConnectionBase* base() const;
#ifndef DISABLE_LIBUV
        bool is_tcp() const { return std::holds_alternative<std::shared_ptr<TCPConnection>>(*this); }
        auto& get_tcp() { return std::get<std::shared_ptr<TCPConnection>>(*this); }
#endif
        bool is_rtc() const { return std::holds_alternative<std::shared_ptr<RTCConnection>>(*this); }
        auto& get_rtc() { return std::get<std::shared_ptr<RTCConnection>>(*this); }
        auto visit(auto lambda) const
        {
            return std::visit(lambda, *this);
        }
        auto visit(auto lambda)
        {
            return std::visit(lambda, *this);
        }
    };
    struct CloseState {
        int error;
    };
    // for inbound connections
    ConnectionBase();
    virtual ~ConnectionBase() {};

    // can be called from all threads
    auto created_at() const { return createdAtSystem; }
    [[nodiscard]] virtual bool is_native() const { return false; }
    std::string to_string() const;
    std::string_view type_str() const;
    uint32_t created_at_timestmap() const { return std::chrono::duration_cast<std::chrono::seconds>(createdAtSystem.time_since_epoch()).count(); }

    // can only be called in eventloop thread because we assume
    // state == MessageState

    virtual void close(int Error) = 0;
    void send(Sndbuffer&& msg);
    [[nodiscard]] std::vector<Rcvbuffer> pop_messages();
    [[nodiscard]] ProtocolVersion protocol_version() const;

    virtual std::shared_ptr<ConnectionBase> get_shared() = 0;

protected:
    // can be called from all threads
    virtual ConnectionVariant get_shared_variant() = 0;
    virtual std::weak_ptr<ConnectionBase> get_weak() = 0;
    virtual uint16_t listen_port() const = 0;
    virtual void async_send(std::unique_ptr<char[]> data, size_t size) = 0;

    // callback methods called from transport implementation thread
    void on_close(const CloseState& cs);
    void on_message(std::span<uint8_t>);
    void on_connected();
    [[nodiscard]] uint16_t asserted_port() const;

private:
    std::span<uint8_t> process_message(std::span<uint8_t> s, std::monostate&);
    std::span<uint8_t> process_message(std::span<uint8_t> s, HandshakeState&);
    std::span<uint8_t> process_message(std::span<uint8_t> s, AckState&);
    std::span<uint8_t> process_message(std::span<uint8_t> s, MessageState&);
    void send_handshake();
    void send_handshake_ack();
    void handshake_timer_start();
    void handshake_timer_expired(); // called from libuv thread

private:
    std::variant<std::monostate, HandshakeState, AckState, MessageState> state;

public:
    const uint64_t id;
    const std::chrono::steady_clock::time_point createdAt;
    const std::chrono::system_clock::time_point createdAtSystem;
};

class AuthenticatableConnection : public ConnectionBase {
public:
    using ConnectionBase::ConnectionBase;
    virtual void start_read() = 0;
};
