#pragma once
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
    friend class PeerState;
    friend class address_manager::AddressManager;
    bool failed_to_connect() const { return !eventloop_registered; }

private:
    std::atomic<bool> eventloop_registered { false };
    bool eventloop_erased { false };
    Coniter dataiter;
};

class PeerServer;

namespace connection_schedule{
    class ConnectionSchedule;
}

struct ConnectRequest {
    using duration = std::chrono::steady_clock::duration;
    friend class ConnectionData;
    static ConnectRequest inbound(EndpointAddress peer)
    {
        return { std::move(peer), -std::chrono::seconds(1) };
    }
    static ConnectRequest outbound(EndpointAddress connectTo, duration sleptFor)
    {
        return { std::move(connectTo), sleptFor };
    }
    auto inbound() const { return sleptFor.count() < 0; }

    const EndpointAddress address;
    const duration sleptFor;

private:
    ConnectRequest(EndpointAddress address, duration sleptFor)
        : address(std::move(address))
        , sleptFor(sleptFor)
    {
    }
};

namespace peerserver {
class ConnectionSchedule;
using duration = std::chrono::steady_clock::duration;


class ConnectionData : public EventloopVariables {
    friend class ::PeerServer;
    friend class connection_schedule::ConnectionSchedule;
    int64_t logrow = 0;

public:
    using duration = std::chrono::steady_clock::duration;
    ConnectionData(const ConnectionData&) = delete;
    ConnectionData(ConnectionData&&) = delete;
    ConnectionData& operator=(const ConnectionData&) = delete;
    ConnectionData& operator=(ConnectionData&&) = delete;
    ConnectionData(const ConnectRequest& cr)
        : connectRequest(cr)
    {
    }
    auto inbound() const { return connectRequest.inbound(); }
    auto peer() const { return connectRequest.address; }
    const ConnectRequest connectRequest;

private:
    bool successfulConnection { false };
};

class Connection : public ConnectionData {
    friend class ::PeerServer;
    virtual void start_read() = 0;

public:
    using ConnectionData::ConnectionData;
};

}
