#pragma once

#include "block/header/batch.hpp"
#include "communication/buffers/sndbuffer.hpp"
#include "eventloop/peer_chain.hpp"
#include "eventloop/sync/block_download/connection_data.hpp"
#include "eventloop/sync/header_download/connection_data.hpp"
#include "eventloop/timer.hpp"
#include "mempool/subscription_declaration.hpp"
#include <deque>

class Timerref {
public:
    Timerref(Timer& t)
        : timer_iter(t.end())
    {
    }
    Timerref(Timer::iterator iter)
        : timer_iter(iter)
    {
    }
    Timer::iterator& timer_ref() { return timer_iter; }
    void reset_expired(Timer& t)
    {
        assert(timer_iter != t.end());
        timer_iter = t.end();
    }
    void reset_notexpired(Timer& t)
    {
        assert(timer_iter != t.end());
        t.cancel(timer_iter);
        timer_iter = t.end();
    }
    bool has_timerref(Timer& timer) { return timer_iter != timer.end(); }
    Timer::iterator timer() { return timer_iter; }

protected:
    Timer::iterator timer_iter;
};

struct ConnectionJob : public Timerref {
    using time_point = std::chrono::steady_clock::time_point;
    using Timerref::Timerref;
    ConnectionJob(uint64_t conId, Timer& t);

    template <typename T>
    requires std::derived_from<T, IsRequest>
    void assign(Timer::iterator iter, Timer& t, T& req)
    {
        assert(!active());
        assert(t.end() != iter);
        timer_iter = iter;
        try {
            assert(!data_v.valueless_by_exception());
            data_v = req;
            assert(data_v.valueless_by_exception() == false);
        } catch (...) {
            assert(0 == 1);
        }
        assert(!data_v.valueless_by_exception());
    }
    bool active() const
    {
        assert(data_v.valueless_by_exception() == false);
        return !std::holds_alternative<std::monostate>(data_v);
    }
    operator bool()
    {
        return active();
    }
    bool waiting_for_init() const
    {
        assert(data_v.valueless_by_exception() == false);
        return std::holds_alternative<AwaitInit>(data_v);
    }

    //////////////////////////////
    // data

    template <typename T>
    void reset_notexpired(Timer& t)
    {
        assert(!data_v.valueless_by_exception());
        Timerref::reset_notexpired(t);
        bool b = !std::holds_alternative<T>(data_v);
        try {
            data_v = std::monostate();
            assert(data_v.valueless_by_exception() == false);
        } catch (...) {
            assert(0 == 1);
        }
        assert(!data_v.valueless_by_exception());
        if (b)
            throw Error(EUNREQUESTED);
    }

    [[nodiscard]] bool awaiting_init() const
    {
        assert(!data_v.valueless_by_exception());
        return std::holds_alternative<AwaitInit>(data_v);
    }

    void restart_expired(Timer::iterator iter, Timer& t)
    {
        assert(timer_iter != t.end());
        timer_iter = iter;
        return;
    }
    using data_t = std::variant<AwaitInit, std::monostate, Proberequest, HeaderRequest, BlockRequest>;
    data_t data_v;

    template <typename T>
    requires std::derived_from<T, WithNonce>
    auto pop_req(T& rep, Timer& t, size_t& activeRequests)
    {
        using type = typename typemap<T>::type;
        assert(!data_v.valueless_by_exception());
        if (!std::holds_alternative<type>(data_v)) {
            throw Error(EUNREQUESTED);
        }
        auto out = std::get<type>(data_v);
        assert(!data_v.valueless_by_exception());
        out.unref_active_requests(activeRequests);
        if (rep.nonce != out.nonce) {
            throw Error(EUNREQUESTED);
        }
        reset_notexpired<type>(t);
        return out;
    }
    void unref_active_requests(size_t& activeRequests)
    {
        assert(!data_v.valueless_by_exception());
        std::visit([&](auto& e) {
            if constexpr (std::is_base_of_v<IsRequest, std::decay_t<decltype(e)>>) {
                e.unref_active_requests(activeRequests);
            }
        },
            data_v);
        assert(!data_v.valueless_by_exception());
    }

private:
    template <typename T>
    struct typemap {
        using type = T;
    };

    template <std::same_as<ProberepMsg> T>
    struct typemap<T> {
        using type = Proberequest;
    };

    template <std::same_as<BatchrepMsg> T>
    struct typemap<T> {
        using type = HeaderRequest;
    };
    template <std::same_as<BlockrepMsg> T>
    struct typemap<T> {
        using type = BlockRequest;
    };
};

struct Ping : public Timerref {
    Ping(Timer& end)
        : Timerref(end)
    {
    }
    void await_pong(PingMsg msg, Timer::iterator iter)
    {
        assert(!data);
        data = msg;
        timer_iter = iter;
    }
    Timer::iterator sleep(Timer::iterator iter)
    {
        auto tmp = timer_iter;
        assert(data);
        data.reset();
        timer_iter = iter;
        return tmp;
    }
    void timer_expired(Timer& timer)
    {
        timer_iter = timer.end();
    }
    PingMsg& check(const PongMsg& m)
    {
        if (!data)
            throw Error(EUNREQUESTED);
        auto e = m.check(*data);
        if (e)
            throw e;
        return *data;
    }

private:
    std::optional<PingMsg> data;
};

struct TimingLog {
    using Milliseconds = std::chrono::milliseconds;
    struct Entry {
        RequestType type;
        Milliseconds duration;
    };
    std::vector<Entry> entries;

    void push(RequestType t, Milliseconds duration)
    {
        push({ t, duration });
    }

    void push(Entry e)
    {
        entries.push_back(std::move(e));
        // periodic prune
        if (entries.size() > 200)
            entries.erase(entries.begin(), entries.begin() + 100);
    }

    auto mean() const
    {
        using namespace std::chrono_literals;
        Milliseconds sum { 0ms };
        for (auto& e : entries) {
            sum += e.duration;
        }
        if (entries.size() > 0)
            sum /= entries.size();
        return sum;
    }
};

struct Loadtest {
    std::optional<RequestType> job;
    [[nodiscard]] std::optional<Request> generate_load(Conref);
};

struct ThrottleDelay {
    using sc = std::chrono::steady_clock;
    sc::duration get() const
    {
        using namespace std::chrono_literals;
        auto n { sc::now() };
        if (n > bucket)
            return 0s;
        auto d { (bucket - n) / 10 };
        return std::min(d, sc::duration(20s));
    }

    sc::time_point add(sc::duration d)
    {
        return bucket = std::max(bucket, sc::now()) + d;
    }

private:
    sc::time_point bucket;
};

template <size_t window>
struct BatchreqThrottler { // throttles if suspicios requests occur
    using seconds = std::chrono::seconds;
    using duration = std::chrono::steady_clock::duration;

private:
    void set_upper(Height upper, size_t spare)
    {
        _u = std::max(upper, _u);
        if (_u.value() + spare >= window)
            _l = std::max(_l, _u + spare - window);
    }

public:
    [[nodiscard]] duration register_request(HeightRange r, size_t spare)
    {
        assert(r.length() > 0);
        set_upper(r.upper(), spare);
        _l = _l + r.length();
        if (_l > _u + spare + 1)
            _l = _u + spare + 1;
        return get_duration(spare);
    }

    [[nodiscard]] duration get_duration(size_t spare) const
    {
        if (_l > _u + spare) {
            return seconds(20);
        }
        return seconds(0);
    }
    auto h0() const { return _l; }
    auto h1() const { return _u; }

private:
    Height _l { Height::zero() };
    Height _u { Height::zero() };
};

struct ThrottleQueue {

    using duration = std::chrono::steady_clock::duration;

    void add_throttle(duration d) { td.add(d); }
    auto reply_delay() const { return td.get(); }
    void insert(messages::Msg, Timer& t, uint64_t connectionId);
    void update_timer(Timer& t, uint64_t connectionId);

    [[nodiscard]] messages::Msg reset_timer_pop_msg()
    {
        assert(timer);
        assert(rateLimitedInput.size() > 0);
        timer.reset();
        auto tmp { std::move(rateLimitedInput.front()) };
        rateLimitedInput.pop_front();
        return tmp;
    }

    BatchreqThrottler<HEADERBATCHSIZE * 20> headerreq;
    BatchreqThrottler<BLOCKBATCHSIZE * 20> blockreq;

private:
    ThrottleDelay td;
    std::deque<messages::Msg> rateLimitedInput;
    std::optional<Timerref> timer;
};

struct Ratelimit {
private:
    using sc = std::chrono::steady_clock;

    template <size_t seconds>
    struct Limiter {
        void operator()()
        {
            tick();
        }
        void tick()
        {
            auto n = sc::steady_clock::now();
            using namespace std::chrono;
            if (n < lastUpdate + std::chrono::seconds(seconds))
                throw Error(EMSGFLOOD);
            lastUpdate = n;
        }

    private:
        sc::time_point lastUpdate = sc::time_point::min();
    };

public:
    Limiter<2 * 60> update;
    Limiter<5> ping;
};

struct Usage {
    Usage(HeaderDownload::Downloader&, BlockDownload::Downloader&);
    ////////////////
    HeaderDownload::ConnectionData data_headerdownload;
    BlockDownload::ConnectionData data_blockdownload;
};

namespace BlockDownload {
class Attorney;
}
struct PeerState {
    PeerState(std::shared_ptr<Connection> c, HeaderDownload::Downloader& h, BlockDownload::Downloader& b, Timer& t);
    std::shared_ptr<Connection> c;
    std::optional<mempool::SubscriptionIter> subscriptionIter;
    ConnectionJob job;
    Height txSubscription { 0 };
    Ratelimit ratelimit;
    ThrottleQueue throttleQueue;
    Loadtest loadtest;
    SignedSnapshot::Priority acknowledgedSnapshotPriority;
    SignedSnapshot::Priority theirSnapshotPriority;
    uint32_t lastNonce;
    bool verifiedEndpoint = false;
    Ping ping;
    Usage usage;
    friend class Eventloop;
    friend class BlockDownload::Downloader;
    friend class BlockDownload::Forks;
    friend class Conref;
    friend class BlockDownload::Attorney;

    bool erased();

private:
    PeerChain chain;
};

bool Conref::operator==(Conref other) const { return data.iter == other.data.iter; }
Conref::operator Connection*()
{
    if (valid())
        return data.iter->second.c.get();
    return nullptr;
}
Conref::operator const Connection*() const
{
    if (valid())
        return data.iter->second.c.get();
    return nullptr;
}
const PeerChain& Conref::chain() const { return data.iter->second.chain; }
PeerChain& Conref::chain() { return data.iter->second.chain; }
const PeerState& Conref::state() const { return data.iter->second; }
PeerState& Conref::state() { return data.iter->second; }
auto& Conref::job() { return data.iter->second.job; }
auto& Conref::job() const { return data.iter->second.job; }
auto& Conref::ping() { return data.iter->second.ping; }
auto Conref::operator->() { return &(data.iter->second); }
bool Conref::initialized() { return !data.iter->second.job.waiting_for_init(); }
inline uint64_t Conref::id() const
{
    return data.iter->first;
}
