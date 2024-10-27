#pragma once
#include "eventloop/types/conref_declaration.hpp"
#include "transport/connect_request.hpp"
#include "transport/helpers/peer_addr.hpp"
#include "transport/helpers/tcp_sockaddr.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>

class AddressManager;
class EventloopVariables {
    friend class Eventloop;
    friend class Conref;
    friend class ConState;
    friend class AddressManager;
    bool failed_to_connect() const { return !eventloop_registered; }

private:
    std::atomic<bool> eventloop_registered { false };
    bool eventloop_erased { false };
    bool addedToSchedule { false };
    Coniter dataiter;
};

class PeerServer;

namespace connection_schedule {
class ConnectionSchedule;
}

namespace tcpconnection_schedule{
class TCPConnectionSchedule;
}

namespace peerserver {
using duration = std::chrono::steady_clock::duration;

class ConnectionData : public EventloopVariables {
    friend class ::PeerServer;
    friend class tcpconnection_schedule::TCPConnectionSchedule;
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
    virtual ~ConnectionData(){}
    virtual bool inbound() const = 0;
    virtual Peeraddr peer_addr() const = 0;
    virtual std::optional<ConnectRequest> connect_request() const = 0;
};

class Connection : public ConnectionData {
    friend class ::PeerServer;

public:
    using ConnectionData::ConnectionData;
    virtual ~Connection(){}
};

}
