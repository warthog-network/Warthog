#pragma once

#include "peerserver/connection_data.hpp"
#include "communication/buffers/recvbuffer.hpp"
#include "eventloop/types/conref_declaration.hpp"
#include "general/tcp_util.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>
class ConnectionBase;
struct EndpointAddress;
namespace uvw {
class timer_handle;
}

struct HandshakeState {
    std::array<uint8_t, 24> recvbuf; // 14 bytes for "WARTHOG GRUNT!" and 4
                                     // bytes for version + 4 extra bytes
                                     // (in case of outbound: + 2 bytes for
                                     //  sending port port)
    std::shared_ptr<uvw::timer_handle> expire_timer;

    static constexpr const char connect_grunt[] = "WARTHOG GRUNT?";
    static constexpr const char accept_grunt[] = "WARTHOG GRUNT!";
    static constexpr const char connect_grunt_testnet[] = "TESTNET GRUNT?";
    static constexpr const char accept_grunt_testnet[] = "TESTNET GRUNT!";
    struct Success {
        uint32_t version;
        uint16_t port;
        std::span<uint8_t> rest;
    };
    auto data() { return recvbuf.data(); }
    size_t size(bool inbound) { return (inbound ? 24 : 22); }
    size_t remaining(bool inbound) { return size(inbound) - pos; }
    uint8_t pos = 0;
    bool handshakesent = false;
    struct Parsed {
        uint32_t version;
        std::optional<uint16_t> port;
    };
    Parsed parse(bool inboound);
    ~HandshakeState();
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
    enum class Type {
        TCP,
        Websocket,
        WebRTC
    };
    struct TCPData {
        static constexpr Type type { Type::TCP };
    };
    struct WebsocketData {
        static constexpr Type type { Type::Websocket };
    };
    struct WebRTCData {
        static constexpr Type type { Type::WebRTC };
    };
    using ConnectionData = std::variant<TCPData, WebsocketData, WebRTCData>;
    struct CloseState {
        int error;
    };
    // for inbound connections
    ConnectionBase(peerserver::ConnectRequest peer);
    virtual ~ConnectionBase() {};

    // can be called from all threads
    auto created_at() const { return createdAtSystem; }
    std::string to_string() const;
    uint32_t created_at_timestmap() const { return std::chrono::duration_cast<std::chrono::seconds>(createdAtSystem.time_since_epoch()).count(); }

    // can only be called in eventloop thread because we assume 
    // state == MessageState 
    EndpointAddress peer_endpoint() const;

    virtual void close(int Error) = 0;
    virtual Type type() const = 0;
    void send(Sndbuffer&& msg);
    [[nodiscard]] std::vector<Rcvbuffer> pop_messages();

protected:
    // can be called from all threads
    virtual std::shared_ptr<ConnectionBase> get_shared() = 0;
    virtual uint16_t listen_port() = 0; // TODO conman.bindAddress.port
    virtual void async_send(std::unique_ptr<char[]> data, size_t size) = 0;

    // callback methods called from transport implementation thread
    void on_close(const CloseState& cs);
    void on_message(std::span<uint8_t>);
    void on_connected();

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
