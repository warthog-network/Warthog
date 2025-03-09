#include "aggregator.hpp"

namespace rxtx {
template <size_t intervalSeconds>
auto Buckets<intervalSeconds>::process(const TransmissionEvent& e, bool in) -> std::vector<agg_t>&
{
    auto peerstr { e.peerHost.to_string() };
    auto end { e.tp.ceil<intervalSeconds>() };
    auto& v { aggregators.try_emplace(peerstr, end).first->second };
    if (v.current.end_time() < end) { // finalize bucket
        v.aggregated.push_back(v.current);
        v.current = { end };
    }
    if (in)
        v.current.rx += e.nBytes;
    else
        v.current.tx += e.nBytes;
    return v.aggregated;
}

template <size_t intervalSeconds>
void Buckets<intervalSeconds>::finalize_all(Timestamp t, bool drain, auto buckets_handler)
{
    auto threshold { t.floor<intervalSeconds>() };
    if (drain) {
        if (lastFinalize >= threshold)
            return;
        lastFinalize = threshold;
    }

    auto it { aggregators.begin() };
    while (it != aggregators.end()) {
        auto& v { it->second };
        if (drain || v.current.end_time() <= threshold) { // close this bucket
            v.aggregated.push_back(v.current);
            buckets_handler(it->first, std::move(v.aggregated));
            aggregators.erase(it++);
        } else {
            if (v.aggregated.size() > 0)
                buckets_handler(it->first, std::move(it->second.aggregated));
            ++it;
        }
    }
}
template <size_t intervalSeconds>
void Buckets<intervalSeconds>::merge_into(std::map<std::string, std::vector<RangeAggregated>>& m) const
{
    for (auto& [host, val] : aggregators) {
        auto& current { val.current };
        auto &v{m[host]};
        if (v.size()>0 && v.back().end_time()==current.end_time() ) {
            v.back().rx+=current.rx;
            v.back().tx+=current.tx;
        }else{
            v.push_back(current);
        }
    }
}
}
