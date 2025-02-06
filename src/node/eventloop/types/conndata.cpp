#include "conndata.hpp"
#include "asyncio/connection.hpp"
#include "eventloop/sync/block_download/block_download.hpp"

using namespace std::chrono_literals;

void Throttled::insert(Sndbuffer sb, Timer& t, uint64_t connectionId)
{
    rateLimitedOutput.push_back(std::move(sb));
    update_timer(t, connectionId);
}

void Throttled::update_timer(Timer& t, uint64_t connectionId)
{
    // update timer if necessary
    if (rateLimitedOutput.size() == 0 || timer.has_value())
        return;
    using namespace std::chrono;
    auto s { duration_cast<seconds>(reply_delay()).count() };
    if (s != 0) {
        spdlog::info("send throttled reply in {} seconds", s); // for debugging
    }
    timer = t.insert(reply_delay(), Timer::ThrottledSend { connectionId });
}

ConnectionJob::ConnectionJob(uint64_t conId, Timer& t)
    : Timerref(t.insert(30s, Timer::CloseNoReply { conId }))
{
}

PeerState::PeerState(std::shared_ptr<Connection> p, HeaderDownload::Downloader& h, BlockDownload::Downloader& b, Timer& t)
    : c(std::move(p))
    , job(c->id, t)
    , ping(t)
    , usage(h, b)
{
}

void Conref::send(Sndbuffer b)
{
    if (!(*this)->c->eventloop_erased)
        data.iter->second.c->asyncsend(std::move(b));
};

Usage::Usage(HeaderDownload::Downloader& h, BlockDownload::Downloader& b)
    : data_headerdownload(h)
    , data_blockdownload(b.focus_end()) { };
