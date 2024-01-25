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
    ForkIter forkIter;
    std::shared_ptr<Descripted> _descripted;
    ForkRange forkRange;

public:
    FocusMap::iterator focusIter;

public:
    ConnectionData(Forkmap::iterator forkEnd, FocusMap::iterator focusEnd)
        : forkIter(forkEnd)
        , focusIter(focusEnd)
    {
    }
    auto& fork_range() const { return forkRange; }
    auto& fork_iter() const { return forkIter; }
    auto& descripted() const { return _descripted; }
};
[[nodiscard]] ConnectionData& data(Conref);
}
