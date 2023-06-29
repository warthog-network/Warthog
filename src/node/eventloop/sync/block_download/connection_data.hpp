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

struct ConnectionData {
    ConnectionData(Forkmap::iterator forkEnd, FocusMap::iterator focusEnd)
        : forkIter(forkEnd)
        , focusIter(focusEnd)
    {
    }
    ForkIter forkIter;
    FocusMap::iterator focusIter;

    std::shared_ptr<Descripted> descripted;
    ForkRange forkRange;
};
}
