#pragma once
#include "general/errors_forward.hpp"
#include "init_arg.hpp"
#include "peerserver/connection_data.hpp"
#include "transport/helpers/ipv4.hpp"
#include "wakeup_time.hpp"
#include <cassert>
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

// enum class ConnectionState {
//     NOT_CONNECTED, // connection failed early
//     CONNECTED_UNINITIALIZED, // connection was closed before init message
//     CONNECTED_INITIALIZED // connection was closed after init message
// };

// struct ReconnectContext {
//     duration prevWait;
//     bool verified;
//     ConnectionState connectionState;
//     bool pinned;
// };

template <typename T>
class SockaddrVectorBase;

class FeelerVector;
class VectorEntry {
public:
    template <typename T>
    friend class SockaddrVectorBase;
    friend class FeelerVector;
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

    void log_success();
    void log_failure();
    json to_json() const;

    Error lastError { 0 };
protected:
    ConnectionLog connectionLog;
    std::set<Source> sources;
    TCPPeeraddr address;
};

class EntryWithTimer : public VectorEntry {
public:
    struct Timer {
        auto sleep_duration() const { return _sleepDuration; }
        auto wakeup_time() const { return wakeupTime; }
        bool active() const { return wakeupTime.has_value(); }
        void deactivate() { wakeupTime.reset(); }
        bool expired_at(time_point tp) const { return wakeupTime && wakeupTime < tp; }
        json to_json() const;
        void set(duration d)
        {
            _sleepDuration = d;
            wakeupTime = steady_clock::now() + d;
        }
        Timer(duration d = duration::zero())
        { // new timers wake up immediately
            set(d);
        }

    private:
        duration _sleepDuration;
        std::optional<time_point> wakeupTime;
    };
    // [[nodiscard]] time_point outbound_connected_ended(const ReconnectContext&);
    // [[nodiscard]] time_point update_timer(const ReconnectContext&);

    std::optional<time_point> make_expired_pending(time_point, std::vector<ConnectRequest>& outpending);
    void wakeup_after(duration);
    json to_json() const;
    [[nodiscard]] std::optional<time_point> wakeup_time() const;
    [[nodiscard]] auto sleep_duration() const { return timer.sleep_duration(); }
    using VectorEntry::VectorEntry;

protected:
    Timer timer;
};

// class ConnectedEntry : public VectorEntry {
// };

class VerifiedEntry : public EntryWithTimer {
public:
    using tp = std::chrono::steady_clock::time_point;

    json to_json() const;
    operator TCPPeeraddr() const { return this->sockaddr(); }
    VerifiedEntry(EntryWithTimer e, tp lastVerified)
        : EntryWithTimer(std::move(e))
        , lastVerified(lastVerified)
    {
    }
    VerifiedEntry(const WithSource<TCPPeeraddr>& i, tp lastVerified)
        : EntryWithTimer(i)
        , lastVerified(lastVerified)
    {
    }
    void on_connected()
    {
        // don't need sources because it we could connect
        sources.clear();
        log_success();
        connections += 1;
    }
    void on_disconnected()
    {
        assert(connections != 0);
        connections -= 1;
    }
    tp lastVerified;
    ssize_t connections { 0 };
};
using ConnectedEntry = VerifiedEntry;

struct TimeoutInfo {
    void update_wakeup_time(const std::optional<time_point>&);
    std::optional<time_point> timeout() const { return wakeup_tp; }
    std::optional<time_point> wakeup_tp;
};
struct Found {
    // void set_
    VectorEntry& match;
    connection_schedule::TimeoutInfo& timeout;
    bool verified;
};

struct FoundDisconnected {
    void wakeup_after(duration);
    EntryWithTimer& match;
    connection_schedule::TimeoutInfo& timeout;
    bool verified;
};

template <typename T>
class SockaddrVectorBase : public TimeoutInfo {
    friend class FeelerVector;
    friend class ConnectionSchedule;

public:
    using elem_t = T;

    [[nodiscard]] elem_t* find(const TCPPeeraddr&) const;
    size_t erase(const TCPPeeraddr& a, auto lambda);
    size_t erase(const TCPPeeraddr& a)
    {
        return erase(a, [](auto) {});
    }
    void take_expired(time_point now, std::vector<ConnectRequest>&);
    elem_t& push_back(elem_t);
    json to_json() const;
    size_t size() const { return data.size(); }
    const auto& elements() const { return data; }

protected:
    mutable std::vector<elem_t> data;
};

class FeelerVector : public SockaddrVectorBase<EntryWithTimer> {

public:
    std::pair<elem_t&, bool> insert(const WithSource<TCPPeeraddr>&);
    std::pair<elem_t&, bool> insert(const EntryWithTimer&);
    using SockaddrVectorBase::SockaddrVectorBase;
};

class ConnectedVector : public SockaddrVectorBase<VectorEntry> {

public:
    std::pair<elem_t&, bool> insert(const WithSource<TCPPeeraddr>&);
    using SockaddrVectorBase::SockaddrVectorBase;
};

class VerifiedVector : public SockaddrVectorBase<VerifiedEntry> {
public:
    using tp = typename VerifiedEntry::tp;
    std::pair<VectorEntry&, bool> insert(const TCPWithSource&, tp lastVerified);
    void prune(auto&& pred, size_t N);
    using SockaddrVectorBase::SockaddrVectorBase;
};

class TCPConnectionSchedule {
    using json = nlohmann::json;
    using InitArg = address_manager::InitArg;
    using ConnectionData = peerserver::ConnectionData;
    // using TCPWithSource = connection_schedule::TCPWithSource;
    // using ConnectionState = connection_schedule::ConnectionState;
    // using time_point = connection_schedule::time_point;
    // using VectorEntry = connection_schedule::VectorEntry;
    // using FeelerVector = connection_schedule::FeelerVector;
    // using ReconnectContext = connection_schedule::ReconnectContext;
    // using Source = connection_schedule::Source;
    // using steady_clock = std::chrono::steady_clock;
    // using VerifiedVector = connection_schedule::VerifiedVector;
    // using ConnectedVector = connection_schedule::ConnectedVector;
    // using Found = connection_schedule::Found;
    // using FoundDisconnected = connection_schedule::FoundDisconnected;
public:
    TCPConnectionSchedule(InitArg);

    void pin(const TCPPeeraddr&);
    void unpin(const TCPPeeraddr&);
    void initialize();
    std::optional<ConnectRequest> add_feeler(TCPPeeraddr, Source);
    void connect_expired();

    // connection callbacks
    void on_outbound_connected(const TCPConnection&);
    void on_outbound_disconnected(const TCPConnectRequest&, Error reason, bool registered);
    void on_outbound_failed(const TCPConnectRequest&, Error reason);
    void on_inbound_disconnected(const IPv4& ip);

    json to_json() const;

    [[nodiscard]] std::optional<time_point> updated_wakeup_time();

    std::vector<TCPPeeraddr> sample_verified(size_t N) const;

private:
    // auto insert_verified(const TCPWithSource&, steady_clock::time_point lastVerified);
    // VectorEntry* move_to_connected(FeelerVector&, const TCPPeeraddr&);
    // void outbound_connection_ended(const ConnectRequest&, ConnectionState state);
    // struct FoundContext : public Found {
    //     ReconnectContext reconnectInfo;
    // };
    // #ifndef DISABLE_LIBUV
    // auto get_context(const TCPConnectRequest&, ConnectionState) -> std::optional<FoundContext>;
    // #endif

    void insert_freshly_pinned(const TCPPeeraddr&);
    void prune_verified();
    [[nodiscard]] std::vector<TCPConnectRequest> pop_expired(time_point now = steady_clock::now());
    void refresh_wakeup_time();
    [[nodiscard]] auto find(const TCPPeeraddr& a) -> std::optional<Found>;
    [[nodiscard]] auto find_disconnected(const TCPPeeraddr& a) -> std::optional<FoundDisconnected>;

    VerifiedVector connectedVerified;
    VerifiedVector disconnectedVerified;
    FeelerVector feelers; // Candidates to test connection to
    //
    size_t totalConnected { 0 };
    PeerServer& peerServer;
    struct ComparatorPinned {
        using is_transparent = void;
        bool operator()(const TCPPeeraddr& a, IPv4 ip) const
        {
            return a.ip < ip;
        }
        bool operator()(IPv4 ip, const TCPPeeraddr& a) const
        {
            return ip < a.ip;
        }
        bool operator()(const TCPPeeraddr& a1, const TCPPeeraddr& a2) const
        {
            return a1 < a2;
        }
    };
    size_t softboundVerified { 1000 };
    std::set<TCPPeeraddr, ComparatorPinned> pinned;
    connection_schedule::WakeupTime wakeup_tp;
};
}

using TCPConnectionSchedule = connection_schedule::TCPConnectionSchedule;
