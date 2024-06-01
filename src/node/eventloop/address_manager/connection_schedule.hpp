#pragma once
#include "peerserver/connection_data.hpp"
#include "transport/helpers/ipv4.hpp"
#include <set>
#include <vector>

namespace connection_schedule {
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
using TCPWithSource = WithSource<TCPSockaddr>;

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

enum class SockaddrState {
    VERIFIED, // could connect at least once to this endpoint
    UNVERIFIED_FAILED, // could never connect and at least one failed try
    UNVERIFIED_NEW // never tried to connect yet
};
enum class ConnectionState {
    NOT_CONNECTED, // connection failed early
    UNINITIALIZED, // connection was closed before init message
    INITIALIZED // connection was closed after init message
};

struct ReconnectContext {
    duration prevWait;
    SockaddrState endpointState;
    ConnectionState connectionState;
    bool pinned;
    bool verified() const { return endpointState == SockaddrState::VERIFIED; }
};

template <typename T>
class SockaddrVectorBase;

class SockaddrVector;
class VectorEntry {
public:
    template <typename T>
    friend class SockaddrVectorBase;
    friend class SockaddrVector;
    std::optional<time_point> activate_if_expired(time_point, std::vector<ConnectRequest>& out);
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
    std::optional<time_point> timeout() const;

    // connection event callbacks
    void connection_established();
    [[nodiscard]] time_point outbound_connected_ended(const ReconnectContext&);

protected:
    [[nodiscard]] time_point update_timer(const ReconnectContext&);

    struct Timer {
        auto sleep_duration() const { return _sleepDuration; }
        auto timeout() const { return _timeout; }
        bool expired(time_point tp) const { return _timeout < tp; }
        void set(duration d)
        {
            _sleepDuration = d;
            _timeout = steady_clock::now() + d;
        }
        Timer()
        { // new timers wake up immediately
            set(duration::zero());
        }

    private:
        duration _sleepDuration;
        time_point _timeout;
    };
    Timer timer;
    ConnectionLog connectionLog;
    std::chrono::steady_clock::time_point lastVerified;
    std::set<Source> sources;
    bool pending { false };
    uint32_t connected { 0 };
    TCPSockaddr address;
};

class VerifiedEntry : public VectorEntry {
public:
    using tp = std::chrono::steady_clock::time_point;

    operator TCPSockaddr() const { return this->sockaddr(); }
    VerifiedEntry(VectorEntry e, tp lastVerified)
        : VectorEntry(std::move(e))
        , lastVerified(lastVerified)
    {
    }
    VerifiedEntry(const WithSource<TCPSockaddr>& i, tp lastVerified)
        : VectorEntry(i)
        , lastVerified(lastVerified)
    {
    }
    tp lastVerified;
};

class VerifiedVector;

template <typename T>
class SockaddrVectorBase {
    friend class SockaddrVector;
    friend class ConnectionSchedule;

public:
    using elem_t = T;

    [[nodiscard]] elem_t* find(const TCPSockaddr&) const;
    void expired_into(time_point now, std::vector<ConnectRequest>&);
    std::optional<time_point> timeout() const { return wakeup_tp; }
    elem_t& push_back(elem_t);

protected:
    VectorEntry& insert(elem_t&&);
    void update_wakeup_time(const std::optional<time_point>&);
    std::optional<time_point> wakeup_tp;
    mutable std::vector<elem_t> data;
};

class SockaddrVector : public SockaddrVectorBase<VectorEntry> {

public:
    void erase(const TCPSockaddr& addr, auto lambda);
    std::pair<elem_t&, bool> emplace(const WithSource<TCPSockaddr>&);
    using SockaddrVectorBase::SockaddrVectorBase;
};

class VerifiedVector : public SockaddrVectorBase<VerifiedEntry> {
public:
    using tp = typename VerifiedEntry::tp;
    std::pair<VectorEntry&, bool> emplace(const TCPWithSource&, tp lastVerified);
    std::vector<TCPSockaddr> sample(size_t N) const;
    using SockaddrVectorBase::SockaddrVectorBase;
};

class WakeupTime {
public:
    auto pop()
    {
        auto tmp { std::move(popped_tp) };
        popped_tp.reset();
        return tmp;
    }
    bool expired() const
    {
        return wakeup_tp < steady_clock::now();
    }
    void reset()
    {
        *this = {};
    }

    auto& val() const { return wakeup_tp; }

    void consider(const std::optional<time_point>& newval)
    {
        if (newval.has_value() && (!wakeup_tp.has_value() || *wakeup_tp > *newval)) {
            wakeup_tp = newval;
            popped_tp = newval;
        }
    }

private:
    std::optional<time_point> wakeup_tp;
    std::optional<time_point> popped_tp;
};

}

class ConnectionSchedule {
    using ConnectionData = peerserver::ConnectionData;
    using TCPWithSource = connection_schedule::TCPWithSource;
    using ConnectionState = connection_schedule::ConnectionState;
    using time_point = connection_schedule::time_point;
    using VectorEntry = connection_schedule::VectorEntry;
    using SockaddrVector = connection_schedule::SockaddrVector;
    using EndpointState = connection_schedule::SockaddrState;
    using ReconnectContext = connection_schedule::ReconnectContext;
    using Source = connection_schedule::Source;
    using steady_clock = std::chrono::steady_clock;
    using VerifiedVector = connection_schedule::VerifiedVector;

public:
    ConnectionSchedule(PeerServer& peerServer, const std::vector<TCPSockaddr>& v);
    void start();
    std::optional<ConnectRequest> insert(TCPSockaddr, Source);
    [[nodiscard]] std::vector<ConnectRequest> pop_expired();
    void connection_established(const TCPConnection&);
    VectorEntry* verify_from(SockaddrVector&, const TCPSockaddr&);
    void outbound_closed(const ConnectionData&);
    void outbound_failed(const ConnectRequest&);
    void schedule_verification(TCPSockaddr c, IPv4 source);

    [[nodiscard]] std::optional<time_point> pop_wakeup_time();

    std::vector<TCPSockaddr> sample_verified(size_t N) const
    {
        return verified.sample(N);
    }
    // template<typename T>
    // std::vector<T> sample_verified(size_t N) const{
    //     return verified.get<T>().sample(N);
    // }

private:
    // auto invoke_with_verified(const TCPSockaddr&, auto lambda) const;
    // auto invoke_with_verified(const TCPSockaddr&, auto lambda);
    auto emplace_verified(const TCPWithSource&, steady_clock::time_point lastVerified);
    VectorEntry* find_verified(const TCPSockaddr&);

    void outbound_connection_ended(const ConnectRequest&, ConnectionState state);
    struct Found {
        VectorEntry& item;
        EndpointState state;
    };
    struct FoundContext {
        VectorEntry& item;
        ReconnectContext context;
    };
    void refresh_wakeup_time();
    auto get_context(const ConnectRequest&, ConnectionState) -> std::optional<FoundContext>;
    [[nodiscard]] auto find(const TCPSockaddr& a) const -> std::optional<Found>;
    VerifiedVector verified;
    SockaddrVector unverifiedNew;
    SockaddrVector unverifiedFailed;
    size_t totalConnected { 0 };
    std::set<TCPSockaddr> pinned;
    connection_schedule::WakeupTime wakeup_tp;
    PeerServer& peerServer;
};
