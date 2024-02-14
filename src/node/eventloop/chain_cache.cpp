#include "chain_cache.hpp"
#include "block/chain/consensus_headers.hpp"
#include "peer_chain.hpp"
#include "types/conndata.hpp"
#include "types/conref_declaration.hpp"

ForkRange& ChaincacheMatch::fork_range(Conref cr) const
{
    using T = ChaincacheMatch::Type;
    ForkRange fr;
    switch (type) {
    case T::CONSENSUS:
        return cr.chain().consensus_fork_range();
    case T::STAGE:
        return cr.chain().stage_fork_range();
    }
    assert(false);
}

StageAndConsensus::StageAndConsensus(const ConsensusSlave& s)
    : consensus(s)
    , scForkHeight(1u, false)
{
    stageHeaders = std::make_shared<Headerchain>();
}

ForkHeight StageAndConsensus::update_stage(Headerchain&& newheaders)
{ // OK
    ForkHeight fh = ::fork_height(*stageHeaders, newheaders);
    stageHeaders = std::make_shared<Headerchain>(std::move(newheaders));
    if ((!scForkHeight.forked()) || fh.val() <= scForkHeight) {
        scForkHeight = ::fork_height(consensus.headers(), *stageHeaders, fh);
    }
    return fh;
}

void StageAndConsensus::stage_clear()
{
    (*stageHeaders) = Headerchain();
    scForkHeight = { 1u, false };
}

AppendMsg StageAndConsensus::update_consensus(Append&& append)
{ // OK
    auto [prevLength, msg] = consensus.apply(std::move(append));
    if (!scForkHeight.forked()) {
        auto startHeight { (prevLength + 1).nonzero_assert() };
        scForkHeight = ::fork_height(consensus.headers(), *stageHeaders, startHeight);
    }
    return msg;
}

ForkMsg StageAndConsensus::update_consensus(Fork&& fork)
{
    auto shrinkLength { fork.shrinkLength };
    auto msg = consensus.apply(std::move(fork));
    if (!scForkHeight.forked() || scForkHeight > shrinkLength) {
        auto startHeight { (shrinkLength + 1).nonzero_assert() };
        scForkHeight = ::fork_height(consensus.headers(), *stageHeaders, startHeight);
    }
    return msg;
}

auto StageAndConsensus::update_consensus(const RollbackData& rd) -> std::optional<SignedPinRollbackMsg>
{
    auto msg { consensus.apply(rd) };
    if (rd.data) {
        auto shrinkLength { rd.data->rollback.shrinkLength };
        if (scForkHeight.forked() && scForkHeight > shrinkLength) {
            scForkHeight = { shrinkLength.value() + 1, false };
        }
    }
    return msg;
}

std::optional<ChaincacheMatch> StageAndConsensus::lookup(std::optional<ChainPin> p) const
{
    using T = enum ChaincacheMatch::Type;
    if (!p.has_value()) {
        if (stageHeaders->length() != 0) {
            return ChaincacheMatch { T::STAGE, stage_pin() };
        }
        if (consensus.headers().length() != 0) {
            return ChaincacheMatch { T::CONSENSUS, consensus_pin() };
        }
        return {};
    }
    assert(stageHeaders);
    if (stageHeaders->length() > p->height) {
        auto bh = stageHeaders->get_header(p->height);
        if (bh && *bh == p->header)
            return ChaincacheMatch { T::STAGE, stage_pin() };
    }
    if (consensus.headers().length() > p->height) {
        auto ch = consensus.headers().get_header(p->height);
        if (ch && ch == p->header)
            return ChaincacheMatch { T::CONSENSUS, consensus_pin() };
    }
    return {};
}

std::optional<HeaderVerifier> StageAndConsensus::header_verifier(const HeaderRange& sb) const
{
    struct Optimizer {
        const HeaderRange& sb;
        struct Optimal {
            const Headerchain* h;
            NonzeroHeight forkHeight;
        };
        std::optional<Optimal> optimal;
        Optimizer(const HeaderRange& sb)
            : sb(sb)
        {
        }
        void consider(const Headerchain& hc)
        {
            auto fh { hc.scan_fork_height(sb) };
            if (fh > sb.offset()) {
                if (!optimal || optimal->forkHeight < fh)
                    optimal = Optimal { &hc, fh };
            }
        }
    };
    Optimizer o { sb };
    o.consider(stage_headers());
    o.consider(consensus.headers());
    if (o.optimal) {
        auto& headerChain { *o.optimal->h };
        Height height { o.optimal->forkHeight - 1 };
        if (height != 0)
            assert(headerChain.get_header(height) == sb.at(height.nonzero_assert()));
        return HeaderVerifier { headerChain, height };
    }
    return {};
}
