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

class EndpointVector;
class EndpointData {
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
    std::set<Source> sources;
    bool pending { false };
    uint32_t connected { 0 };
};

using Sources = std::map<EndpointAddress, EndpointData>;

class EndpointVector {

public:
    [[nodiscard]] EndpointData* find(const EndpointAddress&) const;
    EndpointData* move_entry(const EndpointAddress& key, EndpointVector& to);
    std::pair<EndpointData&, bool> emplace(const EndpointAddressItem&);
    void pop_requests(time_point now, std::vector<ConnectRequest>&);
    bool set_timeout(const EndpointAddress&, time_point tp);
    std::vector<EndpointAddress> sample(size_t N) const;

    std::optional<time_point> timeout() const { return wakeup_tp; }

private:
    EndpointData& insert(const EndpointAddressItem&);
    void update_wakeup_time(const std::optional<time_point>&);
    std::optional<time_point> wakeup_tp;
    mutable std::vector<EndpointData> data;
};

class ConnectionSchedule {
    using ConnectionData = peerserver::ConnectionData;

public:
    [[nodiscard]] std::optional<ConnectRequest> insert(EndpointAddressItem);
    [[nodiscard]] std::vector<ConnectRequest> pop_requests();
    void connection_established(const ConnectionData&);
    void outbound_closed(const ConnectionData&);
    void outbound_failed(const ConnectionData&);
    time_point wake_up_time();
    auto sample_verified(size_t N) const {return verified.sample(N);};


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
    void update_wakeup_time(const std::optional<time_point>&);
    [[nodiscard]] auto find(const EndpointAddress& a) const -> std::optional<Found>;
    EndpointVector verified;
    EndpointVector unverifiedNew;
    EndpointVector unverifiedFailed;
    size_t totalConnected { 0 };
    std::set<EndpointAddress> pinned;
    std::optional<time_point> wakeup_tp;
};
}
