#pragma once
#include "block/chain/header_chain.hpp"
#include "block/header/timestamprule.hpp"
#include "expected.hpp"

struct MiningData {
    Funds reward;
    Hash prevhash;
    Target target;
    uint32_t timestamp;
};

class ExtendableHeaderchain;

class HeaderVerifier {

public:
    struct PreparedAppend {
        const HeaderView hv;
        const Hash hash;
    };
    HeaderVerifier();
    HeaderVerifier(const HeaderVerifier&, const Batch&, Height heightOffset);
    tl::expected<HeaderVerifier, ChainError> copy_apply(const std::optional<SignedSnapshot>& sp, const Batch& b, Height heightOffset) const;
    HeaderVerifier(const SharedBatch&);
    // void clear();
    [[nodiscard]] auto prepare_append(const std::optional<SignedSnapshot>& sp, HeaderView hv) const -> tl::expected<PreparedAppend, int32_t>;

    void initialize(const ExtendableHeaderchain& hc, Height length);
    void append(NonzeroHeight length, const PreparedAppend&);

    // getters
    Height height() const { return length; }
    HashView final_hash() const { return finalHash; };
    auto next_target() const { return nextTarget; }
    auto get_valid_timestamp() const { return std::max(timeValidator.get_valid_timestamp(),latestRetargetTime+1); }

private:
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
    Hash finalHash;
};
class ExtendableHeaderchain : public Headerchain {
    friend class HeaderVerifier;

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
    [[nodiscard]] auto prepare_append(const std::optional<SignedSnapshot>& sp, HeaderView hv) const -> tl::expected<HeaderVerifier::PreparedAppend, int32_t>;

    // Getters
    HashView final_hash() const { return checker.final_hash(); }
    Target next_target() const { return checker.next_target(); }
    MiningData mining_data() const;

protected:
    void initialize();

private:
    HeaderVerifier checker;
};
