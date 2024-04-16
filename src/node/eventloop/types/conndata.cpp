#include "conndata.hpp"
#include "asyncio/tcp/connection.hpp"
#include "eventloop/sync/sync.hpp"
#include "eventloop/types/peer_requests.hpp"

using namespace std::chrono_literals;

ConnectionJob::ConnectionJob(uint64_t conId, Timer& t)
    : Timerref(t.insert(30s, Timer::CloseNoReply { conId }))
{
}

PeerState::PeerState(std::shared_ptr<ConnectionBase> p, HeaderDownload::Downloader& h, BlockDownload::Downloader& b, Timer& t)
    : c(std::move(p))
    , job(c->id, t)
    , ping(t)
    , usage(h, b)
{
}
void Conref::send(Sndbuffer b)
{
    if (!(*this)->c->eventloop_erased) {
        iter->second.c->send(std::move(b));
    }
};

Usage::Usage(HeaderDownload::Downloader& h, BlockDownload::Downloader& b)
    : data_headerdownload(h)
    , data_blockdownload(b.focus_end()) {};
