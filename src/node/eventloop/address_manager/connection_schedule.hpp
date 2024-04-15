#pragma once

#include "general/net/ipv4.hpp"
#include "general/tcp_util.hpp"
#include "peerserver/connection_data.hpp"

#include <chrono>
#include <set>
#include <vector>
namespace connection_schedule {
using Source = IPv4;

using time_point = std::chrono::steady_clock::time_point;
using duration = std::chrono::steady_clock::duration;
using steady_clock = std::chrono::steady_clock;

struct EndpointAddressItem {
    EndpointAddress address;
    Source source;
};

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

enum class EndpointState {
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
    EndpointState endpointState;
    ConnectionState connectionState;
    bool pinned;
    bool verified() const { return endpointState == EndpointState::VERIFIED; }
};

template <typename Elemtype>
class EndpointVectorBase;

class EndpointData {
    template <typename T>
    friend class EndpointVectorBase;
    friend class EndpointVector;

public:
    EndpointData(const EndpointAddressItem& i)
        : address(i.address)
        , sources { i.source }

    {
    }
    void add_source(Source);

    // returns expiration time point
    std::optional<time_point> try_pop(time_point, std::vector<ConnectRequest>& out);
    std::optional<time_point> timeout() const;

    // connection event callbacks
    void connection_established();
    [[nodiscard]] time_point outbound_connected_ended(const ReconnectContext&);
    operator EndpointAddress() const { return address; }

private:
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
    EndpointAddress address;
    Timer timer;
    ConnectionLog connectionLog;
    std::chrono::steady_clock::time_point lastVerified;
    std::set<Source> sources;
    bool pending { false };
    uint32_t connected { 0 };
};



class VerifiedEntry : public EndpointData {
public:
    using tp = std::chrono::steady_clock::time_point;

    VerifiedEntry(const EndpointAddressItem& i, tp lastVerified)
        : EndpointData(i)
        , lastVerified(lastVerified)
    {
    }

    VerifiedEntry(const EndpointData& ed, tp lastVerified)
        : EndpointData(ed)
        , lastVerified(lastVerified)
    {
    }
    tp lastVerified;
};


class VerifiedVector;
template <typename EntryData>
class EndpointVectorBase {
    friend class EndpointVector;

public:
    [[nodiscard]] EntryData* find(const EndpointAddress&) const;
    void pop_requests(time_point now, std::vector<ConnectRequest>&);

    std::optional<time_point> timeout() const { return wakeup_tp; }

protected:
    EndpointData& insert(EntryData&&);
    void update_wakeup_time(const std::optional<time_point>&);
    std::optional<time_point> wakeup_tp;
    mutable std::vector<EntryData> data;
};

class EndpointVector : public EndpointVectorBase<EndpointData> {
public:
    EndpointData* move_entry(const EndpointAddress& key, VerifiedVector& to);
    std::pair<EndpointData&, bool> emplace(const EndpointAddressItem&);
    using EndpointVectorBase<EndpointData>::EndpointVectorBase;
};

class VerifiedVector : public EndpointVectorBase<VerifiedEntry> {
public:
    using tp = VerifiedEntry::tp;
    std::pair<EndpointData&, bool> emplace(const EndpointAddressItem&, tp lastVerified);
    std::vector<EndpointAddress> sample(size_t N) const;
    using EndpointVectorBase<VerifiedEntry>::EndpointVectorBase;
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
    using EndpointAddressItem = connection_schedule::EndpointAddressItem;
    using ConnectionState = connection_schedule::ConnectionState;
    using time_point = connection_schedule::time_point;
    using EndpointData = connection_schedule::EndpointData;
    using EndpointVector = connection_schedule::EndpointVector;
    using VerifiedVector = connection_schedule::VerifiedVector;
    using EndpointState = connection_schedule::EndpointState;
    using ReconnectContext = connection_schedule::ReconnectContext;

public:
    ConnectionSchedule(PeerServer& peerServer, const std::vector<EndpointAddress>& v);
    [[nodiscard]] std::optional<ConnectRequest> insert(EndpointAddressItem);
    [[nodiscard]] std::vector<ConnectRequest> pop_expired();
    void connection_established(const ConnectionData&);
    void outbound_closed(const ConnectionData&);
    void outbound_failed(const ConnectRequest&);
    [[nodiscard]] std::optional<time_point> pop_wakeup_time();
    auto sample_verified(size_t N) const { return verified.sample(N); };

private:
    void outbound_connection_ended(const ConnectRequest&, ConnectionState state);
    struct Found {
        EndpointData& item;
        EndpointState state;
    };
    struct FoundContext {
        EndpointData& item;
        ReconnectContext context;
    };
    void refresh_wakeup_time();
    auto get_context(const ConnectRequest&, ConnectionState) -> std::optional<FoundContext>;
    [[nodiscard]] auto find(const EndpointAddress& a) const -> std::optional<Found>;
    VerifiedVector verified;
    EndpointVector unverifiedNew;
    EndpointVector unverifiedFailed;
    size_t totalConnected { 0 };
    std::set<EndpointAddress> pinned;
    connection_schedule::WakeupTime wakeup_tp;
    PeerServer& peerServer;
};
