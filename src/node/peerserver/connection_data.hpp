#pragma once
#include "asyncio/connect_request.hpp"
#include "asyncio/helpers/socket_addr.hpp"
#include "eventloop/types/conref_declaration.hpp"
#include "general/tcp_util.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>

namespace address_manager {
class AddressManager;
}
class EventloopVariables {
    friend class Eventloop;
    friend class Conref;
    friend class ConnectionSchedule;
    friend class PeerState;
    friend class address_manager::AddressManager;
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

class ConnectionSchedule;
namespace peerserver {
using duration = std::chrono::steady_clock::duration;

class ConnectionData : public EventloopVariables {
    friend class ::PeerServer;
    friend class ::ConnectionSchedule;
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
    virtual Sockaddr connection_peer_addr() const = 0;
    virtual ConnectRequest connect_request() const = 0;
};

class Connection : public ConnectionData {
    friend class ::PeerServer;
    virtual void start_read() = 0;

public:
    using ConnectionData::ConnectionData;
};

}
