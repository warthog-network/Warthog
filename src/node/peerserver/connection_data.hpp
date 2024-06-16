#pragma once
#include "eventloop/types/conref_declaration.hpp"
#include "transport/connect_request.hpp"
#include "transport/helpers/socket_addr.hpp"
#include "transport/helpers/tcp_sockaddr.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>

class AddressManager;
class EventloopVariables {
    friend class Eventloop;
    friend class Conref;
    friend class TCPConnectionSchedule;
    friend class PeerState;
    friend class AddressManager;
    bool failed_to_connect() const { return !eventloop_registered; }

private:
    std::atomic<bool> eventloop_registered { false };
    bool eventloop_erased { false };
    bool successfulConnection { false };
    Coniter dataiter;
};

class PeerServer;

namespace connection_schedule {
class ConnectionSchedule;
}

class TCPConnectionSchedule;
namespace peerserver {
using duration = std::chrono::steady_clock::duration;

class ConnectionData : public EventloopVariables {
    friend class ::PeerServer;
    friend class ::TCPConnectionSchedule;
    int64_t logrow = 0;

public:
    using duration = std::chrono::steady_clock::duration;
    ConnectionData(const ConnectionData&) = delete;
    ConnectionData(ConnectionData&&) = delete;
    ConnectionData& operator=(const ConnectionData&) = delete;
    ConnectionData& operator=(ConnectionData&&) = delete;
    ConnectionData()
    {
    }
    virtual bool inbound() const = 0;
    virtual Sockaddr peer_addr() const = 0;
    virtual std::optional<ConnectRequest> connect_request() const = 0;
};

class Connection : public ConnectionData {
    friend class ::PeerServer;

public:
    using ConnectionData::ConnectionData;
};

}
