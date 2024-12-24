#include "conndata.hpp"
#include "communication/buffers/sndbuffer.hpp"
#include "eventloop/eventloop.hpp"
#include "eventloop/sync/sync.hpp"
#include "eventloop/types/peer_requests.hpp"

using namespace std::chrono_literals;

ConnectionJob::ConnectionJob(uint64_t conId, TimerSystem& t)
    : timer(t.insert(30s, TimerEvent::CloseNoReply { conId }))
{
}

ConState::ConState(std::shared_ptr<ConnectionBase> p, Eventloop& e)
    : c(std::move(p))
    , job(c->id, e.timerSystem)
    , usage(e.headerDownload, e.blockDownload)
{
}
ConState::ConState(std::shared_ptr<ConnectionBase> c, const ConnectionInserter& h)
    : ConState(h.make_connection_state(std::move(c)))
{
}
void Conref::send_buffer(Sndbuffer b)
{
    if (!(*this)->c->eventloop_erased) {
        iter->second.c->send(std::move(b));
    }
};

Usage::Usage(HeaderDownload::Downloader& h, BlockDownload::Downloader& b)
    : data_headerdownload(h)
    , data_blockdownload(b.focus_end()) {};
