#pragma once
#include "general/timestamp.hpp"
#include "transport/helpers/peer_addr.hpp"
#include <cstddef>
#include <map>
#include <vector>

namespace rxtx {
struct TransmissionEvent {
    Timestamp tp;
    Peerhost peerHost;
    size_t nBytes;
    TransmissionEvent(Peerhost peerHost, size_t nBytes)
        : tp(Timestamp::now())
        , peerHost(std::move(peerHost))
        , nBytes(nBytes)
    {
    }
};
struct Transmitted {
    size_t rx { 0 };
    size_t tx { 0 };
};

struct RangeAggregated {
    Timestamp begin;
    Timestamp end;
    size_t rx { 0 };
    size_t tx { 0 };
    Timestamp begin_time() const { return begin; }
    Timestamp end_time() const { return end; }
};

template <size_t stepSeconds>
class RoundedTsAggregated : public Transmitted {
public:
    using ts_t = RoundedTimestamp<stepSeconds>;

private:
    ts_t end;

public:
    operator RangeAggregated() const
    {
        return {
            .begin { begin_time() },
            .end { end_time() },
            .rx { rx },
            .tx { tx }
        };
    }
    auto begin_time() const { return end.prev(); }
    auto end_time() const { return end; }
    RoundedTsAggregated(ts_t end)
        : end(end)
    {
    }
    RoundedTsAggregated(ts_t end, size_t rx, size_t tx)
        : Transmitted(rx, tx)
        , end(end)
    {
    }
};

using MinuteAggregated = RoundedTsAggregated<60>;
using HourAggregated = RoundedTsAggregated<60 * 60>;

template <size_t intervalSeconds>
struct Buckets {
    using agg_t = RoundedTsAggregated<intervalSeconds>;
    using ts_t = agg_t::ts_t;
    struct value_t {
        agg_t current;
        std::vector<agg_t> aggregated;
        value_t(agg_t::ts_t end)
            : current(end)
        {
        }
    };
    ts_t lastFinalize { agg_t::ts_t::zero() };
    std::map<std::string, value_t> aggregators;
    void merge_into(std::map<std::string, std::vector<RangeAggregated>>&) const;
    std::vector<agg_t>& process(const TransmissionEvent&, bool in);
    // handler takes host string as first argument and bucket vector as second argument
    void finalize_all(Timestamp, bool drain, auto buckets_handler);
};

}
