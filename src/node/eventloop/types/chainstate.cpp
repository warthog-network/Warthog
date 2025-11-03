#include "chainstate.hpp"

ConsensusSlave::ConsensusSlave(wrt::optional<SignedSnapshot> sp, Descriptor descriptor, Headerchain headerchain)
    : signedSnapshot(sp)
    , descriptor_(descriptor)
    , headerchain(std::make_shared<Headerchain>(std::move(headerchain)))
{
}

std::pair<Height, AppendMsg> ConsensusSlave::apply(Append&& append)
{
    auto res { headerchain->apply_append(std::move(append.headerchainAppend)) };

    // signed snapshot
    if (append.signedSnapshot) {
        signedSnapshot = append.signedSnapshot;
        assert(signedSnapshot->compatible(*headerchain));
    }
    return res;
}

void ConsensusSlave::update_ratelimit_spare(Height newlength)
{
    assert(newlength < headers().length());
    ratelimitSpare += headers().length() - newlength;
}

ForkMsg ConsensusSlave::apply(Fork&& fork)
{
    assert(descriptor_ + 1 == fork.descriptor);

    update_ratelimit_spare(fork.shrink.length);
    descriptor_ = fork.descriptor;
    if (pinGenerator.use_count() > 1) {
        *pinGenerator = std::move(fork.prevChain);
    }
    pinGenerator.reset();

    // signed snapshot
    if (fork.signedSnapshot) {
        signedSnapshot = fork.signedSnapshot;
        assert(signedSnapshot->compatible(*headerchain));
    }

    ratelimitSpare += headerchain->length() - fork.shrink.length;
    auto res { headerchain->apply_fork(std::move(fork)) };

    return res;
};


auto ConsensusSlave::apply(const RollbackData& rd) -> wrt::optional<SignedPinRollbackMsg>
{
    wrt::optional<SignedPinRollbackMsg> res;

    // signed snapshot
    signedSnapshot = rd.signedSnapshot;

    // rollback
    if (rd.rollback) {
        auto& rollback { rd.rollback->deltaHeaders };
        assert(descriptor_ + 1 == rollback.descriptor);
        descriptor_ = rollback.descriptor;
        headerchain->shrink(rollback.shrink.length);
        update_ratelimit_spare(rollback.shrink.length);

        // prevChain
        if (pinGenerator.use_count() > 1) {
            *pinGenerator = std::move(rd.rollback->prevHeaders);
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
}

Headerchain::pin_t ConsensusSlave::get_pin() const
{
    if (!pinGenerator) {
        pinGenerator = make_shared<std::shared_ptr<Headerchain>>(headerchain);
    }
    return { pinGenerator };
}
const SignedSnapshot::Priority ConsensusSlave::get_signed_snapshot_priority() const
{
    if (signedSnapshot)
        return signedSnapshot->priority;
    return {};
}
