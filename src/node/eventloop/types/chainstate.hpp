#pragma once
#include "block/chain/header_chain.hpp"
#include "chainserver/state/update/update.hpp"
#include <memory>

class ConsensusSlave {
    using Append = chainserver::state_update::Append;
    using Fork = chainserver::state_update::Fork;
    using RollbackData = chainserver::state_update::RollbackData;

public:
    ConsensusSlave(std::optional<SignedSnapshot>, Descriptor descriptor, Headerchain headerchain);
    Worksum total_work() const
    {
        return headerchain->total_work();
    };
    Descriptor descriptor() const { return descriptor_; }
    const Headerchain& headers() const
    {
        return *headerchain;
    }
    Headerchain::pin_t get_pin() const;
    Grid grid() const { return headerchain->grid(); };

    const SignedSnapshot::Priority get_signed_snapshot_priority() const;
    const auto& get_signed_snapshot() const { return signedSnapshot; }

    [[nodiscard]] auto apply(Append&& append) -> std::pair<Height, AppendMsg>;
    [[nodiscard]] auto apply(Fork&& fork) -> ForkMsg;
    [[nodiscard]] auto apply(const RollbackData&) -> std::optional<SignedPinRollbackMsg>;
    [[nodiscard]] auto ratelimit_spare() const { return ratelimitSpare; }

private:
    void update_ratelimit_spare(Height newlength);
    size_t ratelimitSpare { 0 }; // rate limit extra tokens throttling
    std::optional<SignedSnapshot> signedSnapshot;
    Descriptor descriptor_ { 0 };
    std::shared_ptr<Headerchain> headerchain;
    mutable std::shared_ptr<std::shared_ptr<Headerchain>> pinGenerator;
};
