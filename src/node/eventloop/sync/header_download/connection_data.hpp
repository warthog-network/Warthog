#pragma once
#include "types_fwd.hpp"
#include "eventloop/types/probe_data.hpp"

namespace HeaderDownload {

struct ConnectionData {
    ConnectionData(Downloader& d);

private:
    Lead_iter leaderIter;
    struct ProbeInfo : ProbeData {
        std::shared_ptr<Descripted> dsc;
        Queued_iter qiter;
    };

    wrt::optional<ProbeInfo> probeData;
    QueueBatchNode* jobPtr = nullptr;
    wrt::optional<Descriptor> ignoreDescriptor;
    bool mustDisconnect { false };

    friend class Downloader;
};
}
