#include "rogue_headerheights.hpp"

#include "block/chain/header_chain.hpp"

wrt::optional<ChainError> RogueHeaders::find_rogue_in(const HeaderchainSkeleton& hs) const
{
    auto hsr { hs.header_search_recursive() };
    for (auto it { m.rbegin() }; it != m.rend(); ++it) {
        auto h { it->first };
        auto header { hsr.find_prev(h) };
        if (!header)
            break;

        for (auto& e : it->second.vec) {
            if (e.header == *header)
                return ChainError { e.error, h };
        }
    }
    return std::nullopt;
}

bool RogueHeaders::Node::insert(RogueHeaders::NodeValue v)
{
    for (auto& e : vec) {
        if (e.header == v.header)
            return false;
    }
    vec.push_back(v);
    return true;
}

size_t RogueHeaders::Node::prune(const Worksum& minWork)
{
    return std::erase_if(vec, [&](const NodeValue& e) {
        return e.worksum <= minWork;
    });
}

bool RogueHeaders::add(const RogueHeaderData& hh)
{
    // prevent overflow
    if (size() == MAXSIZE)
        clear();

    if (!m[hh.height].insert({ hh.header, hh.worksum, hh.error }))
        return false;
    n += 1;
    return true;
}

size_t RogueHeaders::prune(const Worksum& minWork)
{
    size_t count { 0 };
    for (auto it { m.begin() }; it != m.end();) {
        it->second.prune(minWork);
        // do not keep empty map values
        if (it->second.size() == 0) {
            m.erase(it++);
            count += 1;
        } else
            ++it;
    }
    n -= count;
    return count;
}
