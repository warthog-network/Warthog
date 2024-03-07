#pragma once
#include "eventloop/types/conref_declaration.hpp"
#include "general/tcp_util.hpp"
#include <atomic>
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
namespace peerserver {

class ConnectionData : public EventloopVariables {
    friend class ::PeerServer;
    int64_t logrow = 0;

public:
    ConnectionData(const EndpointAddress& peer, bool inbound)
        : peer(peer)
        , inbound(inbound)
    {
    }
    const EndpointAddress peer;
    const bool inbound;
    std::atomic<bool> b;
};

class Connection : public ConnectionData {
    friend class ::PeerServer;
    virtual void start_read() = 0;

public:
    using ConnectionData::ConnectionData;
};

}
