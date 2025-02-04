#pragma once

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
    using data_t = std::variant<AwaitInit, std::monostate, Proberequest, Batchrequest, Blockrequest>;
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
        using type = Batchrequest;
    };
    template <std::same_as<BlockrepMsg> T>
    struct typemap<T> {
        using type = Blockrequest;
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

struct ThrottleDelay {
    using sc = std::chrono::steady_clock;
    sc::duration get() const
    {
        using namespace std::chrono_literals;
        auto n { sc::now() };
        if (n < bucket)
            return 0s;
        auto d { (n - bucket) / 10 };
        return std::min(d, sc::duration(25s));
    }

    sc::time_point add(sc::duration d)
    {
        return bucket = std::max(bucket, sc::now()) + d;
    }

private:
    sc::time_point bucket;
};

struct Throttled {
    using sc = std::chrono::steady_clock;

    auto reply_delay() const { return td.get(); }
    void insert(Sndbuffer, Timer& t, uint64_t connectionId);
    void update_timer(Timer& t, uint64_t connectionId);

    [[nodiscard]] Sndbuffer reset_timer_get_buf()
    {
        assert(timer);
        assert(rateLimitedOutput.size() > 0);
        timer.reset();
        Sndbuffer tmp { std::move(rateLimitedOutput.front()) };
        rateLimitedOutput.pop_front();
        return tmp;
    }

private:
    ThrottleDelay td;
    std::deque<Sndbuffer> rateLimitedOutput;
    std::optional<Timerref> timer;
};

struct Ratelimit {
    using sc = std::chrono::steady_clock;
    void update() { valid_rate(lastUpdate, std::chrono::minutes(2)); }
    void ping() { return valid_rate(lastUpdate, std::chrono::seconds(5)); }

private:
    void valid_rate(sc::time_point& last, auto duration)
    {
        auto n = sc::steady_clock::now();
        using namespace std::chrono;
        if (n < last + duration)
            throw Error(EMSGFLOOD);
        last = n;
    }
    sc::time_point lastUpdate = sc::time_point::min();
    sc::time_point lastPing = sc::time_point::min();
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
    Throttled throttled;
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
