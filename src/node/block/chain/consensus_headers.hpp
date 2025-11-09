#pragma once
#include "block/chain/header_chain.hpp"
#include "block/header/timestamprule.hpp"
#include "general/result.hpp"
#include "wrt/expected.hpp"

struct MiningData {
    Funds_uint64 reward;
    Hash prevhash;
    Target target;
    uint32_t timestamp;
};

class ExtendableHeaderchain;

class HeaderVerifier {

public:
    struct PreparedAppend {
        const HeaderView hv;
        const BlockHash hash;
    };
    HeaderVerifier();
    HeaderVerifier(const Headerchain& hc, Height length);
    HeaderVerifier(const HeaderVerifier&, const Batch&, Height heightOffset);
    wrt::expected<HeaderVerifier, ChainError> copy_apply(const wrt::optional<SignedSnapshot>& sp, const HeaderSpan&) const;
    HeaderVerifier(const SharedBatch&);
    // void clear();
    [[nodiscard]] auto prepare_append(const wrt::optional<SignedSnapshot>& sp, HeaderView hv, bool verifyPOW = true) const -> Result<PreparedAppend>;

    void append(NonzeroHeight length, const PreparedAppend&);

    // getters
    Height height() const { return length; }
    auto& final_hash() const { return finalHash; };
    auto next_target() const { return nextTarget; }
    auto get_valid_timestamp() const { return std::max(timeValidator.get_valid_timestamp(), latestRetargetTime + 1); }

protected:
    void initialize(const Headerchain& hc, Height length);

private: // data
    Height length { 0 };
    //
    // for retarget computation
    Height latestRetargetHeight { 0 };
    uint32_t latestRetargetTime = 0;
    Target nextTarget;
    //
    // for time validation
    TimestampValidator timeValidator;
    //
    // for prevhash validation
    BlockHash finalHash;
};
class ExtendableHeaderchain : public Headerchain {

public:
    // Constructors
    ExtendableHeaderchain();
    ExtendableHeaderchain(std::vector<Batch>&&, BatchRegistry& br);
    ExtendableHeaderchain(const Headerchain&, Height height);
    ExtendableHeaderchain(Headerchain&&);
    ExtendableHeaderchain(const ExtendableHeaderchain&) = default;

    ExtendableHeaderchain& operator=(Headerchain&&);
    ExtendableHeaderchain& operator=(const ExtendableHeaderchain&) = default;

    // Modifiers
    void shrink(Height newlength); // OK
    void append(const HeaderVerifier::PreparedAppend&, BatchRegistry& br);
    [[nodiscard]] auto prepare_append(const wrt::optional<SignedSnapshot>& sp, HeaderView hv, bool verifyPOW = true) const -> Result<HeaderVerifier::PreparedAppend>;

    // Getters
    const Hash& final_hash() const { return checker.final_hash(); }
    Target next_target() const { return checker.next_target(); }
    MiningData mining_data() const;

protected:
    void initialize();

private:
    HeaderVerifier checker;
};
