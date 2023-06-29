#pragma once

#include "crypto/hash.hpp"
#include <span>

struct TransferView;
struct RewardView;
class AddressView;

class BodyView {
    struct Rewards {
        Rewards(const BodyView& bv)
            : bv(bv)
        {
        }
        size_t offsetRewards = 0;
        struct EndIterator {
        };
        struct Iterator {
            RewardView operator*() const;
            size_t index() { return i; }

            bool operator==(EndIterator) const
            {
                return i >= imax;
            }
            Iterator& operator++()
            {
                i += 1;
                return *this;
            }

        private:
            Iterator(uint16_t i, const BodyView& bv)
                : i(i)
                , imax(bv.getNRewards())
                , bv(bv)
            {
            }
            friend struct Rewards;
            uint16_t i { 0 };
            uint16_t imax;
            const BodyView& bv;
        };
        Iterator begin() { return { 0, bv }; }
        EndIterator end() { return {}; }
        const BodyView& bv;
    };
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
    BodyView(std::span<const uint8_t>);
    Hash merkleRoot() const;
    bool valid() const { return isValid; }
    size_t size() const { return s.size(); }
    const uint8_t* data() const { return s.data(); }

    auto transfers() const { return Transfers { *this }; }
    auto addresses() const { return Addresses { *this }; }
    auto rewards() const { return Rewards { *this }; }
    size_t getNAddresses() const { return nAddresses; };
    TransferView get_transfer(size_t i) const;
    RewardView get_reward(uint16_t i) const;
    AddressView get_address(size_t i) const;

private:
    size_t getNTransfers() const { return nTransfers; };
    uint16_t getNRewards() const { return nRewards; };

private:
    std::span<const uint8_t> s;
    size_t nAddresses;
    uint16_t nRewards;
    size_t nTransfers;
    size_t offsetAddresses = 0;
    size_t offsetRewards = 0;
    size_t offsetTransfers = 0;
    bool isValid = false;
};
