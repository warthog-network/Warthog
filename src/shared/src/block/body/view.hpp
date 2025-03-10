#pragma once

#include "block/body/container.hpp"
#include "block/chain/height.hpp"
#include "block/header/header.hpp"
#include "crypto/hash.hpp"
#include "defi/token/id.hpp"
#include <span>

struct TokenCreationView;
struct WartTransferView;
struct RewardView;
class AddressView;
class BodyView;

class BodyStructure {
    friend BodyView;
    struct TokenSection {
        size_t beginOffset;
        TokenId tokenId;
        size_t nTransfers;
        size_t transfersOffset;
        size_t nOrders;
        size_t ordersOffset;
        size_t nLiquidityAdd;
        size_t liquidityAddBegin;
        size_t nLiquidityRemove;
        size_t liquidityRemoveOffset;
    };
    struct TokenSectionView {
        const TokenSection& ts;
        const uint8_t* dataBody;
        auto foreach_transfer(auto lambda) const;
        auto foreach_order(auto lambda) const;
        auto foreach_liquidity_add(auto lambda) const;
        auto foreach_liquidity_remove(auto lambda) const;
    };
    BodyStructure() { };

public:
    static std::optional<BodyStructure> parse(std::span<const uint8_t> s, NonzeroHeight h, BlockVersion version);
    static BodyStructure parse_throw(std::span<const uint8_t> s, NonzeroHeight h, BlockVersion version);
    constexpr static size_t SIGLEN { 65 };
    constexpr static size_t AddressSize { 20 };
    constexpr static size_t RewardSize { 16 };
    constexpr static size_t TransferSize { 34 + SIGLEN };
    constexpr static size_t OrderSize { 26 + SIGLEN };
    constexpr static size_t LiquidityAddSize { 34 + SIGLEN };
    constexpr static size_t LiquidityRemoveSize { 26 + SIGLEN };
    constexpr static size_t TokenCreationSize { 8 + 8 + 5 + 2 + SIGLEN };

private:
    size_t nAddresses;
    size_t nTransfers { 0 };
    size_t nTokens { 0 };
    std::vector<TokenSection> tokensSections;
    size_t nNewTokens { 0 };
    size_t offsetAddresses { 0 };
    size_t offsetReward { 0 };
    size_t offsetTransfers { 0 };
    size_t offsetTokens { 0 };
    size_t offsetNewTokens { 0 };
};

class BodyView {
    friend class BodyView;
    struct Addresses {
        Addresses(const BodyView& bv)
            : bv(bv)
        {
        }
        size_t offsetAddresses = 0;
        struct EndIterator {
        };
        struct Iterator {
            AddressView operator*() const;
            size_t index() { return i; }

            bool operator==(EndIterator) const
            {
                return i == imax;
            }
            Iterator& operator++()
            {
                i += 1;
                return *this;
            }

        private:
            Iterator(size_t i, const BodyView& bv)
                : i(i)
                , imax(bv.getNAddresses())
                , bv(bv)
            {
            }
            friend struct Addresses;
            size_t i { 0 };
            size_t imax;
            const BodyView& bv;
        };
        Iterator begin() { return { 0, bv }; }
        EndIterator end() { return {}; }
        const BodyView& bv;
    };
    struct Transfers {
        Transfers(const BodyView& bv)
            : bv(bv)
        {
        }
        size_t offsetAddresses = 0;
        struct EndIterator {
        };
        struct Iterator {
            WartTransferView operator*() const;
            size_t index() { return i; }

            bool operator==(EndIterator) const
            {
                return i == imax;
            }
            Iterator& operator++()
            {
                i += 1;
                return *this;
            }

        private:
            Iterator(size_t i, const BodyView& bv)
                : i(i)
                , imax(bv.getNTransfers())
                , bv(bv)
            {
            }
            friend struct Transfers;
            size_t i { 0 };
            size_t imax;
            const BodyView& bv;
        };
        Iterator begin() { return { 0, bv }; }
        EndIterator end() { return {}; }
        const BodyView& bv;
    };

    struct NewTokens {
        NewTokens(const BodyView& bv)
            : bv(bv)
        {
        }

        struct EndIterator {
        };
        struct Iterator {
            TokenCreationView operator*() const;
            size_t index() { return i; }

            bool operator==(EndIterator) const
            {
                return i == imax;
            }
            Iterator& operator++()
            {
                i += 1;
                return *this;
            }

        private:
            Iterator(size_t i, const BodyView& bv)
                : i(i)
                , imax(bv.getNNewTokens())
                , bv(bv)
            {
            }
            friend struct NewTokens;
            size_t i { 0 };
            size_t imax;
            const BodyView& bv;
        };
        Iterator begin() { return { 0, bv }; }
        EndIterator end() { return {}; }

        const BodyView& bv;
    };

public:
    using TokenSectionView = BodyStructure::TokenSectionView;
    BodyView(const BodyContainer& bodyContainer, const BodyStructure& bodyStructure)
        : bodyContainer(bodyContainer)
        , bodyStructure(bodyStructure) { };
    std::vector<Hash> merkle_leaves() const;
    Hash merkle_root(Height h) const;
    std::vector<uint8_t> merkle_prefix() const;
    size_t size() const { return bodyContainer.size(); }
    const uint8_t* data() const { return bodyContainer.data().data(); }

    auto transfers() const { return Transfers { *this }; }
    inline auto foreach_token(auto lambda) const;
    auto addresses() const { return Addresses { *this }; }
    auto token_creations() const { return NewTokens { *this }; }
    size_t getNAddresses() const { return bodyStructure.nAddresses; };
    size_t getNNewTokens() const { return bodyStructure.nNewTokens; };
    WartTransferView get_transfer(size_t i) const;
    TokenCreationView get_new_token(size_t i) const;
    RewardView reward() const;
    Funds_uint64 fee_sum_assert() const;
    AddressView get_address(size_t i) const;

private:
    size_t getNTransfers() const { return bodyStructure.nTransfers; };

private:
    const BodyContainer& bodyContainer;
    const BodyStructure& bodyStructure;
};
