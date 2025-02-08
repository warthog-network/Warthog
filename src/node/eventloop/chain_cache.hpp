#pragma once
#include "block/chain/fork_range.hpp"
#include "block/chain/pin.hpp"
#include "communication/messages.hpp"
#include "types/chainstate.hpp"

class PeerChain;
class Conref;
class ChaincacheMatch {
private:
    friend class StageAndConsensus;
    enum class Type { STAGE,
        CONSENSUS } type;

public:
    ChaincacheMatch(Type type, Headerchain::pin_t pin)
        : type(type)
        , pin(std::move(pin))
    {
    }
    [[nodiscard]] ForkRange& fork_range(Conref cr) const;
    Headerchain::pin_t pin;
};

class StageAndConsensus {
    using Append = chainserver::state_update::Append;
    using Fork = chainserver::state_update::Fork;
    using RollbackData = chainserver::state_update::RollbackData;

public:
    StageAndConsensus(const ConsensusSlave& s);

    // update functions
    [[nodiscard]] auto update_stage(Headerchain&&) -> ForkHeight;
    [[nodiscard]] auto update_consensus(Append&&) -> AppendMsg;
    [[nodiscard]] auto update_consensus(Fork&&) -> ForkMsg;
    [[nodiscard]] auto update_consensus(const RollbackData&) -> std::optional<SignedPinRollbackMsg>;

    // const lookup functions
    [[nodiscard]] std::optional<ChaincacheMatch> lookup(std::optional<ChainPin>) const;
    auto consensus_length() const { return consensus.headers().length(); }
    auto consensus_state() const -> const auto& { return consensus; }
    auto stage_headers() const -> const Headerchain& { return *stageHeaders; }
    void stage_clear();
    const auto& signed_snapshot() const { return consensus.get_signed_snapshot(); }
    ForkHeight fork_height() const { return scForkHeight; }
    [[nodiscard]] std::optional<HeaderVerifier> header_verifier(const HeaderRange&) const;

    // pin header chains
    [[nodiscard]] Headerchain::pin_t stage_pin() const
    {
        return { std::make_shared<std::shared_ptr<Headerchain>>(stageHeaders) };
    }
    auto consensus_pin() const
    {
        return consensus.get_pin();
    }
    auto ratelimit_spare() const { return consensus.ratelimit_spare(); }

private:
    Headerchain::pin_t pin;
    std::shared_ptr<Headerchain> stageHeaders;
    ConsensusSlave consensus;
    ForkHeight scForkHeight; // fork height of stageHeaders <-> consensus
};
