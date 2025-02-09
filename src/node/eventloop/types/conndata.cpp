#include "conndata.hpp"
#include "asyncio/connection.hpp"
#include "eventloop/sync/block_download/block_download.hpp"

using namespace std::chrono_literals;

void ThrottleQueue::insert(messages::Msg sb, Timer& t, uint64_t connectionId)
{
    rateLimitedInput.push_back(std::move(sb));
    if (rateLimitedInput.size() > 15)
        throw Error(EMSGFLOOD);
    update_timer(t, connectionId);
}

void ThrottleQueue::update_timer(Timer& t, uint64_t connectionId)
{
    // update timer if necessary
    if (rateLimitedInput.size() == 0 || timer.has_value())
        return;
    using namespace std::chrono;
    auto s { duration_cast<seconds>(reply_delay()).count() };
    if (s != 0) {
        spdlog::info("send throttled reply in {} seconds", s); // for debugging
    }
    timer = t.insert(reply_delay(), Timer::ThrottledProcessMsg { connectionId });
}

namespace {

template <std::same_as<HeaderRequest> T>
std::optional<Request> gen_load(Conref cr)
{
    auto descripted { cr.chain().descripted() };
    return HeaderRequest(descripted, Batchslot(descripted->chain_length()),
        1, descripted->worksum());
}

template <std::same_as<BlockRequest> T>
std::optional<Request> gen_load(Conref cr)
{
    auto& d { cr.chain().descripted() };
    Height h { d->chain_length() };
    if (h.is_zero())
        return std::nullopt; // cannot send block request to this peer

    auto l { d->chain_length() };
    NonzeroHeight lower {
        l.value() + 1 > BLOCKBATCHSIZE ? (l + 1 - BLOCKBATCHSIZE).nonzero_assert() : NonzeroHeight(1u)
    };
    NonzeroHeight upper { l };

    return BlockRequest {
        d, BlockRange { lower, upper }
    };
}
}

std::optional<Request> Loadtest::generate_load(Conref cr)
{
    if (!job.has_value())
        return {};
    return job.value().visit([&](auto r) {
        using type = decltype(r)::type;
        return gen_load<type>(cr);
    });
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
