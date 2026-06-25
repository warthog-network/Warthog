#include "chain_cache.hpp"
#include "block/chain/consensus_headers.hpp"
#include "peer_chain.hpp"
#include "types/conndata.hpp"
#include "types/conref_declaration.hpp"

ForkRange& ChaincacheMatch::fork_range(Conref cr) const
{
    using enum Type;
    switch (type) {
    case CONSENSUS:
        return cr.chain().consensus_fork_range();
    case STAGE:
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
    auto shrinkLength { fork.shrink.length };
    auto msg = consensus.apply(std::move(fork));
    if (!scForkHeight.forked() || scForkHeight > shrinkLength) {
        auto startHeight { (shrinkLength + 1).nonzero_assert() };
        scForkHeight = ::fork_height(consensus.headers(), *stageHeaders, startHeight);
    }
    return msg;
}

auto StageAndConsensus::update_consensus(const SignedSnapshotApply& rd) -> std::optional<SignedPinRollbackMsg>
{
    auto msg { consensus.apply(rd) };
    if (rd.rollback) {
        auto shrinkLength { rd.rollback->deltaHeaders.shrink.length };
        if (scForkHeight.forked() && scForkHeight > shrinkLength) {
            scForkHeight = { shrinkLength.value() + 1, false };
        }
    }
    return msg;
}

void StageAndConsensus::update_consensus(const Rollback& rd)
{
    consensus.apply(rd);
    if (rd.rollback) {
        auto shrinkLength { rd.rollback->deltaHeaders.shrink.length };
        if (scForkHeight.forked() && scForkHeight > shrinkLength) {
            scForkHeight = { shrinkLength.value() + 1, false };
        }
    }
}

std::optional<ChaincacheMatch> StageAndConsensus::lookup(std::optional<ChainPin> p) const
{
    using enum ChaincacheMatch::Type;
    assert(stageHeaders); // is set in constructor.
    if (!p.has_value()) {
        if (stageHeaders->length() != 0) {
            return ChaincacheMatch { STAGE, stage_pin() };
        }
        if (consensus.headers().length() != 0) {
            return ChaincacheMatch { CONSENSUS, consensus_pin() };
        }
        return {};
    }
    if (stageHeaders->length() > p->height) {
        auto bh = stageHeaders->get_header(p->height);
        if (bh && *bh == p->header)
            return ChaincacheMatch { STAGE, stage_pin() };
    }
    if (consensus.headers().length() > p->height) {
        auto ch = consensus.headers().get_header(p->height);
        if (ch && ch == p->header)
            return ChaincacheMatch { CONSENSUS, consensus_pin() };
    }
    return {};
}

std::optional<HeaderVerifier> StageAndConsensus::header_verifier(const HeaderSpan& hs) const
{
    struct Optimizer {
        const HeaderSpan& hs;
        struct Optimal {
            const Headerchain* h;
            NonzeroHeight matchHeight;
        };
        std::optional<Optimal> optimal;
        Optimizer(const HeaderSpan& hs)
            : hs(hs)
        {
        }
        void consider(const Headerchain& hc)
        {
            auto mh { hc.max_match_height(hs) };
            if (mh.has_value()) {
                if (!optimal || optimal->matchHeight < *mh)
                    optimal = Optimal { &hc, *mh };
            }
        }
    };
    Optimizer o { hs };
    o.consider(stage_headers());
    o.consider(consensus.headers());
    if (o.optimal) {
        auto& headerChain { *o.optimal->h };
        NonzeroHeight height { o.optimal->matchHeight };
        assert(headerChain.get_header(height) == hs.at(height));
        return HeaderVerifier { headerChain, height };
    }
    return {};
}
