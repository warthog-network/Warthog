#pragma once

#include "block/header/header.hpp"
#include "eventloop/types/probe_data.hpp"
#include <list>
#include <map>
#include <memory>

// forward declaration
namespace HeaderDownload {
class Downloader;
struct LeaderNode;
using Lead_list = std::list<LeaderNode>;
using Lead_iter = Lead_list::iterator;
struct QueueBatchNode;
using Queued_iter = std::map<Header, QueueBatchNode>::iterator;

struct ConnectionData {
    ConnectionData(HeaderDownload::Downloader& d);

private:
    HeaderDownload::Lead_iter leaderIter;
    struct ProbeInfo : ProbeData {
        std::shared_ptr<Descripted> dsc;
        HeaderDownload::Queued_iter qiter;
    };
    std::optional<ProbeInfo> probeData;
    HeaderDownload::QueueBatchNode* jobPtr = nullptr;
    std::optional<Descriptor> ignoreDescriptor;
    bool mustDisconnect { false };

    friend class HeaderDownload::Downloader;
};
}
