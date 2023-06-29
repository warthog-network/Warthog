#include "chainstate.hpp"

ConsensusSlave::ConsensusSlave(std::optional<SignedSnapshot> sp, Descriptor descriptor, Headerchain headerchain)
    : signedSnapshot(sp)
    , descriptor_(descriptor)
    , headerchain(std::make_shared<Headerchain>(std::move(headerchain)))
{
    return;
};

std::pair<Height, AppendMsg> ConsensusSlave::apply(Append&& append)
{
    auto res { headerchain->apply_append(std::move(append.headerchainAppend)) };

    // signed snapshot
    if (append.signedSnapshot) {
        signedSnapshot = append.signedSnapshot;
        assert(signedSnapshot->compatible(*headerchain));
    }
    return res;
};

ForkMsg ConsensusSlave::apply(Fork&& fork)
{
    descriptor_ = fork.descriptor;
    if (pinGenerator.use_count() > 1) {
        *pinGenerator = std::move(fork.prevChain);
    }
    pinGenerator.reset();
    auto res { headerchain->apply_fork(std::move(fork)) };

    // signed snapshot
    if (fork.signedSnapshot) {
        signedSnapshot = fork.signedSnapshot;
        assert(signedSnapshot->compatible(*headerchain));
    }
    return res;
};

auto ConsensusSlave::apply(const RollbackData& rd) -> std::optional<SignedPinRollbackMsg>
{
    std::optional<SignedPinRollbackMsg> res;

    // signed snapshot
    signedSnapshot = rd.signedSnapshot;

    // rollback
    if (rd.data) {
        descriptor_ = rd.data->rollback.descriptor;
        headerchain->shrink(rd.data->rollback.shrinkLength);

        // prevChain
        if (pinGenerator.use_count() > 1) {
            *pinGenerator = std::move(rd.data->prevChain);
        }
        pinGenerator.reset();

        // set result
        res = SignedPinRollbackMsg {
            *signedSnapshot,
            headerchain->length(),
            headerchain->total_work(),
            descriptor_
        };
    }

    assert(signedSnapshot->compatible(*headerchain));
    return res;
};

Headerchain::pin_t ConsensusSlave::get_pin() const
{
    if (!pinGenerator) {
        pinGenerator = make_shared<std::shared_ptr<Headerchain>>(headerchain);
    }
    return { pinGenerator };
};
const SignedSnapshot::Priority ConsensusSlave::get_signed_snapshot_priority() const { 
    if (signedSnapshot) 
        return signedSnapshot->priority;
    return {};
}
