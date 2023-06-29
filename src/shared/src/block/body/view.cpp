#include "view.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hasher_sha256.hpp"
#include "general/hex.hpp"
#include "general/reader.hpp"
#include <iostream>
#include <vector>
using namespace std;

BodyView::BodyView(std::span<const uint8_t> s)
    : s(s)
{
    static_assert(SIGLEN == RecoverableSignature::length);
    Reader rd { s };
    if (s.size() > MAXBLOCKSIZE)
        return;
    // Read new address section
    if (rd.remaining() < 8)
        return;
    rd.skip(4); // for mining
    nAddresses = rd.uint32();
    offsetAddresses = rd.cursor() - s.data();
    if (rd.remaining() < nAddresses * AddressSize + 4)
        return;
    rd.skip(nAddresses * AddressSize);

    // Read reward section
    nRewards = rd.uint16();
    if (rd.remaining() < RewardSize * nRewards + 4)
        return;
    offsetRewards = rd.cursor() - s.data();
    rd.skip(16 * nRewards);

    // Read payment section
    nTransfers = rd.uint32();
    // Make sure that it has correct length
    if (rd.remaining() != (TransferSize)*nTransfers)
        return;
    offsetTransfers = rd.cursor() - s.data();
    isValid = true;
};

Hash BodyView::merkleRoot() const
{
    assert(isValid);
    std::vector<Hash> hashes(nAddresses + nRewards + nTransfers);

    // hash addresses
    size_t idx = 0;
    for (size_t i = 0; i < nAddresses; ++i)
        hashes[idx++] = hashSHA256(s.data() + offsetAddresses + i * AddressSize, AddressSize);

    // hash payouts
    for (size_t i = 0; i < nRewards; ++i)
        hashes[idx++] = hashSHA256(data() + offsetRewards + i * RewardSize, RewardSize);
    // hash payments
    for (size_t i = 0; i < nTransfers; ++i)
        hashes[idx++] = hashSHA256(data() + offsetTransfers + i * TransferSize, TransferSize);
    std::vector<Hash> tmp, *from, *to;
    from = &hashes;
    to = &tmp;
    bool includedSeed = false;
    bool finish = false;
    do {
        to->resize((from->size() + 1) / 2);
        if (from->size() <= 2 && !includedSeed) {
            HasherSHA256 hasher {};
            hasher.write((*from)[0].data(), 32);
            if (1 < from->size()) {
                hasher.write((*from)[1].data(), 32);
            }
            hasher.write(data(), 4);
            includedSeed = true;
            (*to)[0] = std::move(hasher);
        } else {
            if (from->size() == 1)
                finish = true;
            size_t j = 0;
            for (size_t i = 0; i < (from->size() + 1) / 2; ++i) {
                HasherSHA256 hasher {};
                hasher.write((*from)[j].data(), 32);
                if (j + 1 < from->size()) {
                    hasher.write((*from)[j + 1].data(), 32);
                }
                (*to)[i] = std::move(hasher);
                j += 2;
            }
        }
        std::swap(from, to);
    } while (!finish);
    return from->front();
}
