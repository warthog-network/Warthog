#pragma once
#include "block/chain/fork_range.hpp"
#include "block/chain/height.hpp"
#include <map>
class Conref;
namespace BlockDownload {
using Forkmap = std::multimap<NonzeroHeight, Conref>;
using ForkIter = Forkmap::iterator;
struct BlockSlot;
struct FocusNode;
using FocusMap = std::map<BlockSlot, FocusNode>; // indexed by batch index
class Downloader;
class Forks;
class Focus;
class ConnectionData {
    friend class Forks;

private:
    class ForkData {
        std::shared_ptr<Descripted> _descripted;
        struct IterRange {
            ForkIter _iter;
            ForkRange _range;
            IterRange(ForkIter iter, ForkRange range);
        } iterRange;

    public:
        ForkIter iter() const { return iterRange._iter; }
        auto range() const { return iterRange._range; }
        auto& descripted() const { return _descripted; }
        void udpate_iter_range(ForkIter iter, ForkRange range);
        ForkData(std::shared_ptr<Descripted> d, ForkIter fi, ForkRange forkRange);
    };
    wrt::optional<ForkData> forkData;

public:
    FocusMap::iterator focusIter;

public:
    ConnectionData(FocusMap::iterator focusEnd)
        : focusIter(focusEnd)
    {
    }
    bool has_fork_data() const { return forkData.has_value(); }
    ConnectionData(const ConnectionData&) = delete;
    ConnectionData(ConnectionData&&) = default;
    auto fork_range() const { return forkData.value().range(); }
    auto fork_iter() const { return forkData.value().iter(); }
    auto& descripted() const { return forkData.value().descripted(); }
};
[[nodiscard]] ConnectionData& data(Conref);
}
