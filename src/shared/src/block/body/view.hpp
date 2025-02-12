#pragma once

#include "block/chain/height.hpp"
#include "crypto/hash.hpp"
#include "defi/token/id.hpp"
#include <span>

struct TokenCreationView;
struct WartTransferView;
struct RewardView;
class AddressView;

class BodyView {
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

    struct TokenSection {
        const uint8_t* begin;
        TokenId tokenId;
        size_t nTransfers;
        const uint8_t* transfersBegin;
        size_t nOrders;
        const uint8_t* ordersBegin;
        size_t nLiquidityAdd;
        const uint8_t* liquidityAddBegin;
        size_t nLiquidityRemove;
        const uint8_t* liquidityRemoveBegin;
        auto foreach_transfer(auto lambda) const;
        auto foreach_order(auto lambda) const;
        auto foreach_liquidity_add(auto lambda) const;
        auto foreach_liquidity_remove(auto lambda) const;
    };

public:
    constexpr static size_t SIGLEN { 65 };
    constexpr static size_t AddressSize { 20 };
    constexpr static size_t RewardSize { 16 };
    constexpr static size_t TransferSize { 34 + SIGLEN };
    constexpr static size_t OrderSize { 26 + SIGLEN };
    constexpr static size_t LiquidityAddSize { 34 + SIGLEN };
    constexpr static size_t LiquidityRemoveSize { 26 + SIGLEN };
    constexpr static size_t TokenCreationSize { 8 + 8 + 5 + 2 + SIGLEN };
    BodyView(std::span<const uint8_t>, NonzeroHeight h);
    std::vector<Hash> merkle_leaves() const;
    Hash merkle_root(Height h) const;
    std::vector<uint8_t> merkle_prefix() const;
    bool valid() const { return isValid; }
    size_t size() const { return s.size(); }
    const uint8_t* data() const { return s.data(); }

    auto transfers() const { return Transfers { *this }; }
    auto foreach_token(auto lambda) const;
    auto addresses() const { return Addresses { *this }; }
    auto token_creations() const { return NewTokens { *this }; }
    size_t getNAddresses() const { return nAddresses; };
    size_t getNNewTokens() const { return nNewTokens; };
    WartTransferView get_transfer(size_t i) const;
    TokenCreationView get_new_token(size_t i) const;
    RewardView reward() const;
    Funds fee_sum_assert() const;
    AddressView get_address(size_t i) const;

private:
    size_t getNTransfers() const { return nTransfers; };

private:
    std::span<const uint8_t> s;
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
    bool isValid = false;
};
