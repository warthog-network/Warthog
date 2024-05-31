#pragma once

#include "eventloop/peer_chain.hpp"
#include "eventloop/sync/block_download/connection_data.hpp"
#include "eventloop/sync/header_download/connection_data.hpp"
#include "eventloop/timer.hpp"
#include "mempool/subscription_declaration.hpp"
#include "peer_requests.hpp"
#include "rtc/peer_rtc_state.hpp"
#include "transport/connection_base.hpp"

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
    // requires T::is_reply // TODO
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
        if (rep.nonce() != out.nonce()) {
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
    struct PingV2Data {
        struct ExtraData {
            ExtraData(uint64_t signalingListDiscardIndex)
                : signalingListDiscardIndex(signalingListDiscardIndex)
            {
            }
            uint64_t signalingListDiscardIndex;
        } extraData;
        PingV2Msg msg;
    };
    Ping(Timer& end)
        : Timerref(end)
    {
    }
    void await_pong(PingMsg msg, Timer::iterator iter)
    {
        assert(!has_value());
        data = std::move(msg);
        timer_iter = iter;
    }
    void await_pong_v2(PingV2Data d, Timer::iterator iter)
    {
        assert(!has_value());
        data = std::move(d);
        timer_iter = iter;
    }
    Timer::iterator sleep(Timer::iterator iter)
    {
        auto tmp = timer_iter;
        assert(has_value());
        data = std::monostate();
        timer_iter = iter;
        return tmp;
    }
    void timer_expired(Timer& timer)
    {
        timer_iter = timer.end();
    }
    auto& check(const PongV2Msg& m)
    {
        if (!std::holds_alternative<PingV2Data>(data))
            throw Error(EUNREQUESTED);
        auto& d { std::get<PingV2Data>(data) };
        auto e = m.check(d.msg);
        if (e)
            throw e;
        return d;
    }
    PingMsg& check(const PongMsg& m)
    {
        if (!std::holds_alternative<PingMsg>(data))
            throw Error(EUNREQUESTED);
        auto& d { std::get<PingMsg>(data) };
        auto e = m.check(d);
        if (e)
            throw e;
        return d;
    }

private:
    bool has_value() const { return !std::holds_alternative<std::monostate>(data); }
    std::variant<std::monostate, PingMsg, PingV2Data> data;
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

class PeerState {
public:
    PeerState(std::shared_ptr<ConnectionBase> c, HeaderDownload::Downloader& h, BlockDownload::Downloader& b, Timer& t);
    std::shared_ptr<ConnectionBase> c;
    std::optional<mempool::SubscriptionIter> subscriptionIter;
    ConnectionJob job;
    Height txSubscription { 0 };
    Ratelimit ratelimit;
    PeerRTCState rtcState;
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

inline bool Conref::operator==(const Conref& cr) const { return iter == cr.iter; }
inline const PeerChain& Conref::chain() const { return iter->second.chain; }
inline PeerChain& Conref::chain() { return iter->second.chain; }
inline auto& Conref::job() { return iter->second.job; }
inline auto& Conref::job() const { return iter->second.job; }
inline auto& Conref::rtc() { return iter->second.rtcState; }
inline auto Conref::peer() const { return iter->second.c->peer_addr(); }
inline auto& Conref::ping() { return iter->second.ping; }
inline auto Conref::operator->() { return &(iter->second); }
inline auto Conref::version() const{ return iter->second.c->protocol_version();}
inline bool Conref::initialized() const { return !iter->second.job.waiting_for_init(); }
inline bool Conref::is_native() const { return iter->second.c->is_native(); }
inline uint64_t Conref::id() const
{
    return iter->first;
}
