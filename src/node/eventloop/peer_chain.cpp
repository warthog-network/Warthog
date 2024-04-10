#include "peer_chain.hpp"
#include "block/chain/binary_forksearch.hpp"
#include "block/chain/header_chain.hpp"
#include "chain_cache.hpp"
#include "general/errors.hpp"
void PeerChain::initialize(const InitMsg& msg, const StageAndConsensus& sac)
{
    if (msg.chainLength.complete_batches() != msg.grid.size()) {
        throw Error(EINV_INITGRID);
    }
    desc = std::make_shared<Descripted>(
        msg.descriptor,
        msg.chainLength,
        msg.worksum,
        msg.grid);
    auto& d = *desc.get();
    consensusForkRange = ForkRange { sac.consensus_state().headers(), d.grid() };
    stageForkRange = ForkRange { sac.stage_headers(), desc->grid() };
    priority = msg.sp;
}

void PeerChain::on_peer_append(const AppendMsg& msg, const StageAndConsensus& sac)
{
    assert(desc);
    auto& d = *desc.get();
    d.append_throw(msg);
    consensusForkRange.on_append(*desc, sac.consensus_state().headers());
    stageForkRange.on_append(*desc, sac.stage_headers());
}

void PeerChain::on_peer_fork(const ForkMsg& msg, const StageAndConsensus& sac)
{
    assert(desc);
    desc->deprecate();
    if (msg.descriptor != desc->descriptor + 1) {
        throw Error(EDESCRIPTOR);
    }

    auto newgrid { desc->grid() };
    newgrid.shrink((msg.forkHeight - 1).complete_batches());
    newgrid.append(msg.grid);

    desc = std::make_shared<Descripted>(
        msg.descriptor,
        msg.chainLength,
        msg.worksum,
        newgrid);

    consensusForkRange.on_fork(msg.forkHeight, *desc, sac.consensus_state().headers());
    stageForkRange.on_fork(msg.forkHeight, *desc, sac.stage_headers());
}

void PeerChain::on_peer_shrink(const SignedPinRollbackMsg& msg, const StageAndConsensus& sac)
{
    assert(desc);
    desc->deprecate();
    if (msg.descriptor != desc->descriptor + 1) {
        throw Error(EDESCRIPTOR);
    }

    auto newgrid { desc->grid() };
    newgrid.shrink((msg.shrinkLength).complete_batches());

    desc = std::make_shared<Descripted>(
        msg.descriptor,
        msg.shrinkLength,
        msg.worksum,
        newgrid);

    consensusForkRange.on_shrink(*desc, sac.consensus_state().headers());
    stageForkRange.on_shrink(*desc, sac.stage_headers());
}

void PeerChain::on_consensus_fork(NonzeroHeight forkHeight, const StageAndConsensus& sac)
{
    consensusForkRange.on_fork(forkHeight, *desc, sac.consensus_state().headers());
    auto matchHeight { std::min(sac.fork_height().val(), stageForkRange.lower()) - 1 };
    consensusForkRange.on_match(matchHeight);
}

void PeerChain::on_stage_fork(NonzeroHeight forkHeight, const Headerchain& blockdownloadHeaders)
{
    stageForkRange.on_fork(forkHeight, *desc, blockdownloadHeaders);
}

void PeerChain::on_consensus_append(const StageAndConsensus& sac)
{
    consensusForkRange.on_append(*desc, sac.consensus_state().headers());
    auto matchHeight { std::min(sac.fork_height().val(), stageForkRange.lower()) - 1 };
    consensusForkRange.on_match(matchHeight);
}

void PeerChain::on_stage_append_or_shrink(const Headerchain& blockdownloadHeaders)
{
    stageForkRange.on_append_or_shrink(*desc, blockdownloadHeaders);
}

void PeerChain::on_consensus_shrink(const StageAndConsensus& sac)
{
    consensusForkRange.on_shrink(*desc, sac.consensus_state().headers());
}

void PeerChain::on_stage_set(Height length)
{
    stageForkRange = ForkRange((length + 1).nonzero_assert());
}

PeerchainMatch PeerChain::on_proberep(const ProbereqMsg& req, const ProberepMsg& msg, const StageAndConsensus& sac) // OK
{
    using enum PeerchainMatch;
    PeerchainMatch res { NOMATCH };
    const auto& fh = sac.fork_height();
    if (msg.currentDescriptor != desc->descriptor)
        throw ChainError { EPROBEDESCRIPTOR, Height(msg.currentDescriptor.value()+1).nonzero_assert() };
    if (desc->chain_length() < req.height) {
        // should not have msg.current
        if (msg.current.has_value())
            throw ChainError { EBADPROBE, req.height };
        return res;
    }
    if (!msg.current.has_value())
        throw ChainError { EBADPROBE, req.height };

    auto& consensus = sac.consensus_state().headers();
    // check consensus match
    if (consensus.length() >= req.height) {
        if (consensus[req.height] == *msg.current) {
            res = CONSENSUSMATCH;
            consensusForkRange.on_match(req.height);
            if (fh <= req.height) {
                stageForkRange.on_match(fh.val() - 1);
                if (fh <= sac.stage_headers().length())
                    stageForkRange.on_mismatch(fh);
            }
        } else {
            consensusForkRange.on_mismatch(req.height);
        }
    }

    // check blockdownload match
    if (sac.stage_headers().length() >= req.height) {
        if (sac.stage_headers()[req.height] == *msg.current) {
            res = BLOCKDOWNLOADMATCH;
            stageForkRange.on_match(req.height);
            if (fh <= req.height) {
                consensusForkRange.on_match(fh.val() - 1);
                if (fh <= consensus.length())
                    consensusForkRange.on_mismatch(fh);
            }
        } else {
            stageForkRange.on_mismatch(req.height);
        }
    }
    return res;
}
