#pragma once
#include "general/errors_forward.hpp"
#include "init_arg.hpp"
#include "peerserver/connection_data.hpp"
#include "transport/helpers/ipv4.hpp"
#include "wakeup_time.hpp"
#include <nlohmann/json_fwd.hpp>
#include <set>
#include <vector>

struct TCPConnectRequest;

namespace connection_schedule {
using json = nlohmann::json;

using Source = IPv4;

using time_point = std::chrono::steady_clock::time_point;
using duration = std::chrono::steady_clock::duration;
using steady_clock = std::chrono::steady_clock;

template <typename addr_t>
struct WithSource {
    addr_t address;
    std::optional<Source> source;
    explicit WithSource(addr_t addr)
        : address(std::move(addr)) {};
    WithSource(addr_t addr, Source source)
        : address(std::move(addr))
        , source(std::move(source)) {};
};
using TCPWithSource = WithSource<TCPPeeraddr>;

// data structure to encode success of recent connection tries
class ConnectionLog {
public:
    [[nodiscard]] size_t consecutive_failures() const;
    [[nodiscard]] bool last_connection_failed() const;
    void log_failure(); // returns number of repeated failures
    void log_success();

private:
    uint32_t active_bits() const
    {
        return bits & 0x1F;
    }
    uint32_t bits { 1 << 5 };
};

enum class VerificationState {
    VERIFIED, // could connect at least once to this endpoint
    UNVERIFIED, // could never connect and at least one failed try
    FEELER // never tried to connect yet
};
enum class ConnectionState {
    NOT_CONNECTED, // connection failed early
    CONNECTED_UNINITIALIZED, // connection was closed before init message
    CONNECTED_INITIALIZED // connection was closed after init message
};

struct ReconnectContext {
    duration prevWait;
    VerificationState endpointState;
    ConnectionState connectionState;
    bool pinned;
    bool verified() const { return endpointState == VerificationState::VERIFIED; }
};

template <typename T>
class SockaddrVectorBase;

class SockaddrVector;
class VectorEntry {
public:
    template <typename T>
    friend class SockaddrVectorBase;
    friend class SockaddrVector;
    std::optional<time_point> make_expired_pending(time_point, std::vector<ConnectRequest>& outpending);
    auto sockaddr() const { return address; };
    VectorEntry(const TCPWithSource& i)
        : address(i.address)
    {
        if (i.source.has_value()) {
            sources.insert(*i.source);
        }
    }
    void add_source(Source);

    // returns expiration time point
    std::optional<time_point> wakeup_time() const;

    // connection event callbacks
    void connection_established();
    json to_json() const;
    [[nodiscard]] time_point outbound_connected_ended(const ReconnectContext&);

protected:
    [[nodiscard]] time_point update_timer(const ReconnectContext&);

    struct Timer {
        auto sleep_duration() const { return _sleepDuration; }
        auto wakeup_time() const { return _wakeupTime; }
        bool expired_at(time_point tp) const { return _wakeupTime < tp; }
        json to_json() const;
        void set(duration d)
        {
            _sleepDuration = d;
            _wakeupTime = steady_clock::now() + d;
        }
        Timer()
        { // new timers wake up immediately
            set(duration::zero());
        }

    private:
        duration _sleepDuration;
        time_point _wakeupTime;
    };
    Timer timer;
    ConnectionLog connectionLog;
    std::set<Source> sources;
    bool active { false };
    TCPPeeraddr address;
};

class VerifiedEntry : public VectorEntry {
public:
    using tp = std::chrono::steady_clock::time_point;

    json to_json() const;
    operator TCPPeeraddr() const { return this->sockaddr(); }
    VerifiedEntry(VectorEntry e, tp lastVerified)
        : VectorEntry(std::move(e))
        , lastVerified(lastVerified)
    {
    }
    VerifiedEntry(const WithSource<TCPPeeraddr>& i, tp lastVerified)
        : VectorEntry(i)
        , lastVerified(lastVerified)
    {
    }
    tp lastVerified;
};

class VerifiedVector;

struct VectorTimeout {
    void update_wakeup_time(const std::optional<time_point>&);
    std::optional<time_point> wakeup_tp;
};

template <typename T>
class SockaddrVectorBase : public VectorTimeout {
    friend class SockaddrVector;
    friend class ConnectionSchedule;

public:
    using elem_t = T;

    [[nodiscard]] elem_t* find(const TCPPeeraddr&) const;
    void take_expired(time_point now, std::vector<ConnectRequest>&);
    std::optional<time_point> timeout() const { return wakeup_tp; }
    elem_t& push_back(elem_t);
    json to_json() const;

protected:
    VectorEntry& insert(elem_t&&);
    mutable std::vector<elem_t> data;
};

class SockaddrVector : public SockaddrVectorBase<VectorEntry> {

public:
    void erase(const TCPPeeraddr& addr, auto lambda);
    std::pair<elem_t&, bool> emplace(const WithSource<TCPPeeraddr>&);
    using SockaddrVectorBase::SockaddrVectorBase;
};

class VerifiedVector : public SockaddrVectorBase<VerifiedEntry> {
public:
    using tp = typename VerifiedEntry::tp;
    std::pair<VectorEntry&, bool> emplace(const TCPWithSource&, tp lastVerified);
    std::vector<TCPPeeraddr> sample(size_t N) const;
    using SockaddrVectorBase::SockaddrVectorBase;
};
class TCPSchedule {
};

}

class TCPConnectionSchedule {
    using json = nlohmann::json;
    using InitArg = address_manager::InitArg;
    using ConnectionData = peerserver::ConnectionData;
    using TCPWithSource = connection_schedule::TCPWithSource;
    using ConnectionState = connection_schedule::ConnectionState;
    using time_point = connection_schedule::time_point;
    using VectorEntry = connection_schedule::VectorEntry;
    using SockaddrVector = connection_schedule::SockaddrVector;
    using VerificationState = connection_schedule::VerificationState;
    using ReconnectContext = connection_schedule::ReconnectContext;
    using Source = connection_schedule::Source;
    using steady_clock = std::chrono::steady_clock;
    using VerifiedVector = connection_schedule::VerifiedVector;

public:
    TCPConnectionSchedule(InitArg);

    void start();
    std::optional<ConnectRequest> insert(TCPPeeraddr, Source);
    void connect_expired();
    void outbound_established(const TCPConnection&);
    void outbound_closed(const TCPConnectRequest&, bool success, Error reason);
    void outbound_failed(const TCPConnectRequest&);
    json to_json() const;

    [[nodiscard]] std::optional<time_point> pop_wakeup_time();

    std::vector<TCPPeeraddr> sample_verified(size_t N) const
    {
        return verified.sample(N);
    }

private:
    [[nodiscard]] std::vector<TCPConnectRequest> pop_expired(time_point now = steady_clock::now());
    auto emplace_verified(const TCPWithSource&, steady_clock::time_point lastVerified);
    VectorEntry* move_to_verified(SockaddrVector&, const TCPPeeraddr&);
    VectorEntry* find_verified(const TCPPeeraddr&);

    void outbound_connection_ended(const ConnectRequest&, ConnectionState state);
    struct Found {
        VectorEntry& match;
        connection_schedule::VectorTimeout& timeout;
        VerificationState verificationState;
    };
    struct FoundContext : public Found {
        ReconnectContext reconnectInfo;
    };
    void refresh_wakeup_time();
#ifndef DISABLE_LIBUV
    auto get_context(const TCPConnectRequest&, ConnectionState) -> std::optional<FoundContext>;
#endif
    [[nodiscard]] auto find(const TCPPeeraddr& a) -> std::optional<Found>;
    VerifiedVector verified;
    SockaddrVector feelers;
    SockaddrVector unverified;
    size_t totalConnected { 0 };
    PeerServer& peerServer;
    std::set<TCPPeeraddr> pinned;
    connection_schedule::WakeupTime wakeup_tp;
};
