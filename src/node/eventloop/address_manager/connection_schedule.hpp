#pragma once

#include "peerserver/connection_data.hpp"
#include "transport/helpers/ipv4.hpp"

#include <chrono>
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

template <typename EntryData, typename addr_t>
class SockaddrVectorBase;

class VectorEntryBase {

public:
    VectorEntryBase(Source source)
        : sources { source }
    {
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
};

class SockaddrVector;

template <typename addr_t>
class VectorEntry : public VectorEntryBase {
public:
    template <typename, typename>
    friend class SockaddrVectorBase;
    friend class SockaddrVector;
    std::optional<time_point> try_pop(time_point, std::vector<ConnectRequest>& out);
    auto sockaddr() const { return address; };
    VectorEntry(const WithSource<addr_t>& i)
        : VectorEntryBase(i.source)
        , address(i.address)
    {
    }
    VectorEntry(VectorEntryBase base, addr_t addr)
        : VectorEntryBase(std::move(base))
        , address(std::move(addr))
    {
    }

private:
    addr_t address;
};

template <typename addr_t>
class VerifiedEntry : public VectorEntry<addr_t> {
public:
    using tp = std::chrono::steady_clock::time_point;

    operator addr_t() const { return this->sockaddr(); }
    VerifiedEntry(const WithSource<addr_t>& i, tp lastVerified)
        : VectorEntry<addr_t>(i)
        , lastVerified(lastVerified)
    {
    }

    VerifiedEntry(VectorEntryBase ed, addr_t addr, tp lastVerified)
        : VectorEntry<addr_t>(std::move(ed), std::move(addr))
        , lastVerified(lastVerified)
    {
    }
    tp lastVerified;
};

template <typename addr_t>
class VerifiedVector;

template <typename T, typename addr_t>
class SockaddrVectorBase {
    friend class SockaddrVector;
    friend class ConnectionSchedule;

public:
    using elem_t = T;

    [[nodiscard]] elem_t* find(const addr_t&) const;
    void pop_requests(time_point now, std::vector<ConnectRequest>&);
    std::optional<time_point> timeout() const { return wakeup_tp; }
    elem_t& push_back(elem_t);

protected:
    VectorEntry<addr_t>& insert(elem_t&&);
    void update_wakeup_time(const std::optional<time_point>&);
    std::optional<time_point> wakeup_tp;
    mutable std::vector<elem_t> data;
};

class SockaddrVector : public SockaddrVectorBase<VectorEntry<Sockaddr>, Sockaddr> {

public:
    void erase(const Sockaddr& addr, auto lambda);
    std::pair<elem_t&, bool> emplace(const WithSource<Sockaddr>&);
    using SockaddrVectorBase::SockaddrVectorBase;
};

template <typename addr_t>
class VerifiedVector : public SockaddrVectorBase<VerifiedEntry<addr_t>, addr_t> {
public:
    using tp = typename VerifiedEntry<addr_t>::tp;
    std::pair<VectorEntry<addr_t>&, bool> emplace(const WithSource<addr_t>&, tp lastVerified);
    std::vector<addr_t> sample(size_t N) const;
    using SockaddrVectorBase<VerifiedEntry<addr_t>, addr_t>::SockaddrVectorBase;
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
    template <typename addr_t>
    using WithSource = connection_schedule::WithSource<addr_t>;
    using ConnectionState = connection_schedule::ConnectionState;
    using time_point = connection_schedule::time_point;
    using VectorEntryBase = connection_schedule::VectorEntryBase;
    using SockaddrVector = connection_schedule::SockaddrVector;
    using EndpointState = connection_schedule::SockaddrState;
    using ReconnectContext = connection_schedule::ReconnectContext;
    using Source = connection_schedule::Source;
    using steady_clock = std::chrono::steady_clock;
    template<typename T>
    using VerifiedVector = connection_schedule::VerifiedVector<T>;

    template <typename T>
    class VerifiedVectors { };

    template <typename T1, typename... Ts>
    struct VerifiedVectors<std::variant<T1, Ts...>> : public VerifiedVector<T1>,
                                       public VerifiedVectors<std::variant<Ts...>> {
        template <typename T>
        [[nodiscard]] VerifiedVector<T> & get()
        {
            return *this;
        }
        void pop_requests(time_point now, std::vector<ConnectRequest>& out){
            get<T1>().pop_requests(now,out);
            VerifiedVectors<std::variant<Ts...>>::pop_requests(now,out);
        }
        std::optional<time_point> timeout() const { 
            return std::min(get<T1>().timeout(),
            VerifiedVectors<std::variant<Ts...>>::timeout());
        }
    };

    template <typename T1>
    struct VerifiedVectors<std::variant<T1>> : public VerifiedVector<T1> {
    public:
        template <typename T>
        [[nodiscard]] const VerifiedVector<T> & get() const
        {
            return *this;
        }
        template <typename T>
        [[nodiscard]] VerifiedVector<T> & get()
        {
            return *this;
        }
        void pop_requests(time_point now, std::vector<ConnectRequest>& out){
            get<T1>().pop_requests(now,out);
        }
        std::optional<time_point> timeout() const { 
            return get<T1>().timeout();
        }

    };


public:
    ConnectionSchedule(PeerServer& peerServer, const std::vector<Sockaddr>& v);
    void start();
    std::optional<ConnectRequest> insert(Sockaddr, Source);
    [[nodiscard]] std::vector<ConnectRequest> pop_expired();
    void connection_established(const ConnectionData&);
    VectorEntryBase* move_entry(SockaddrVector&, const Sockaddr&);
    void outbound_closed(const ConnectionData&);
    void outbound_failed(const ConnectRequest&);
    void schedule_verification(TCPSockaddr c, IPv4 source);

    [[nodiscard]] std::optional<time_point> pop_wakeup_time();
    std::vector<TCPSockaddr> sample_verified_tcp(size_t N) const;

private:
    auto invoke_with_verified(const Sockaddr&, auto lambda) const;
    auto invoke_with_verified(const Sockaddr&, auto lambda);
    auto emplace_verified(const WithSource<Sockaddr>&, steady_clock::time_point lastVerified);
    VectorEntryBase* find_verified(const Sockaddr&);

    void outbound_connection_ended(const ConnectRequest&, ConnectionState state);
    struct Found {
        VectorEntryBase& item;
        EndpointState state;
    };
    struct FoundContext {
        VectorEntryBase& item;
        ReconnectContext context;
    };
    void refresh_wakeup_time();
    auto get_context(const ConnectRequest&, ConnectionState) -> std::optional<FoundContext>;
    [[nodiscard]] auto find(const Sockaddr& a) const -> std::optional<Found>;
    VerifiedVectors<Sockaddr::variant_t> verified;
    SockaddrVector unverifiedNew;
    SockaddrVector unverifiedFailed;
    size_t totalConnected { 0 };
    std::set<Sockaddr> pinned;
    connection_schedule::WakeupTime wakeup_tp;
    PeerServer& peerServer;
};
