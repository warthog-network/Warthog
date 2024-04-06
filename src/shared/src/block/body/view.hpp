#pragma once

#include "block/chain/height.hpp"
#include "crypto/hash.hpp"
#include <span>

struct TransferView;
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
            TransferView operator*() const;
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

public:
    constexpr static size_t SIGLEN { 65 };
    constexpr static size_t AddressSize { 20 };
    constexpr static size_t RewardSize { 16 };
    constexpr static size_t TransferSize { 34 + SIGLEN };
    BodyView(std::span<const uint8_t>, NonzeroHeight h);
    std::vector<Hash> merkle_leaves() const;
    Hash merkle_root(Height h) const;
    std::vector<uint8_t> merkle_prefix() const;
    bool valid() const { return isValid; }
    size_t size() const { return s.size(); }
    const uint8_t* data() const { return s.data(); }

    auto transfers() const { return Transfers { *this }; }
    auto addresses() const { return Addresses { *this }; }
    size_t getNAddresses() const { return nAddresses; };
    TransferView get_transfer(size_t i) const;
    RewardView reward() const;
    Funds fee_sum() const;
    AddressView get_address(size_t i) const;

private:
    size_t getNTransfers() const { return nTransfers; };

private:
    std::span<const uint8_t> s;
    size_t nAddresses;
    uint16_t nRewards;
    size_t nTransfers { 0 };
    size_t offsetAddresses = 0;
    size_t offsetReward = 0;
    size_t offsetTransfers = 0;
    bool isValid = false;
};
