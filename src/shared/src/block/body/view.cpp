#include "view.hpp"
#include "block/body/parse.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hasher_sha256.hpp"
#include "defi/token/id.hpp"
#include "general/is_testnet.hpp"
#include "general/reader.hpp"
#include <vector>
using namespace std;

namespace block {
template <size_t elem_size>
Section<elem_size>::Section(Reader& r, size_t n)
    : offset(r.offset())
    , n(n)
{
    r.skip(n * elem_size);
}
}

std::optional<BodyStructure> BodyStructure::parse(std::span<const uint8_t> s, NonzeroHeight h, BlockVersion version)
{
    BodyStructure bs;
    static_assert(SIGLEN == RecoverableSignature::length);
    Reader rd { s };
    bool block_v4 { version.value() >= 4 };
    bool block_v2 = is_testnet() || h.value() >= NEWBLOCKSTRUCUTREHEIGHT;

    if (s.size() > MAXBLOCKSIZE)
        return {};
    const bool defiEnabled { true };
    if (block_v2) {
        // Read new address section
        rd.skip(10); // for mining
        bs.nAddresses = rd.uint16();
        bs.offsetAddresses = rd.offset();
        if (rd.remaining() < bs.nAddresses * AddressSize)
            return {};
        rd.skip(bs.nAddresses * AddressSize);

        // Read reward section
        if (rd.remaining() < RewardSize)
            return {};
        bs.offsetReward = rd.offset();
        rd.skip(16);

        // Read payment section
        if (rd.remaining() != 0) {
            bs.nTransfers = rd.uint32();
            // Make sure that it has correct length
            if (rd.remaining() < (TransferSize)*bs.nTransfers)
                return {};
        }
        bs.offsetTransfers = rd.offset();
        rd.skip(bs.nTransfers * TransferSize);

        if (block_v4) {
            // read token sections
            bs.nTokens = rd.uint16();
            for (size_t i = 0; i < bs.nTokens; ++i) {
                size_t beginOffset { rd.offset() };
                TokenId tokenId { rd.uint32() };
                auto rd10 { // lambda to read 10 bits from rd
                    [bits = size_t(0), nbits = 0u, &rd]() mutable -> size_t {
                        while (nbits < 10) {
                            bits <<= 8;
                            bits |= rd.uint8();
                            nbits += 8;
                        }
                        auto excess { nbits - 10 };
                        auto res { bits >> excess };
                        bits &= ((1 << excess) - 1);
                        nbits -= 10;
                        return res;
                    }
                };

                // 7 bytes for 4 10-bit length specifications
                size_t nTransfers { rd10() };
                size_t nOrders { rd10() };
                size_t nLiquidityAdd { rd10() };
                size_t nLiquidityRemove { rd10() };
                size_t nCancelations { rd10() };

                bs.tokensSections.push_back(
                    TokenSection {
                        .beginOffset = beginOffset,
                        .tokenId = tokenId,
                        .transfers { rd, nTransfers },
                        .orders { rd, nOrders },
                        .liquidityAdd { rd, nLiquidityAdd },
                        .liquidityRemove { rd, nLiquidityRemove },
                        .cancelations { rd, nCancelations },
                    });
            }

            // read new token section
            if (defiEnabled && rd.remaining() != 0) {
                bs.nNewTokens = rd.uint8();
            }
            bs.offsetNewTokens = rd.offset();
        }
    } else {
        // Read new address section
        if (rd.remaining() < 8)
            return {};
        rd.skip(4); // for mining
        bs.nAddresses = rd.uint32();
        bs.offsetAddresses = rd.offset();
        if (rd.remaining() < bs.nAddresses * AddressSize + 4)
            return {};
        rd.skip(bs.nAddresses * AddressSize);

        // Read reward section
        rd.skip(2);
        if (rd.remaining() < RewardSize + 4)
            return {};
        bs.offsetReward = rd.offset();
        rd.skip(16);

        // Read payment section
        bs.nTransfers = rd.uint32();
        // Make sure that it has correct length
        if (rd.remaining() != (TransferSize)*bs.nTransfers)
            return {};
        bs.offsetTransfers = rd.offset();
    }
    return bs;
};

BodyStructure BodyStructure::parse_throw(std::span<const uint8_t> s, NonzeroHeight h, BlockVersion version)
{
    if (auto p { parse(s, h, version) })
        return *p;
    throw Error(EINV_BODY);
}

std::vector<Hash> BodyView::merkle_leaves() const
{
    auto& bs { bodyStructure };
    std::vector<Hash> hashes;
    hashes.reserve(bs.nAddresses + 1 + bs.nTransfers + bs.nTokens);
    auto add_hash { [&](const uint8_t* data, size_t len) {
        hashes.push_back(hashSHA256(data, len));
    } };

    // hash addresses
    for (size_t i = 0; i < bs.nAddresses; ++i)
        add_hash(data() + bs.offsetAddresses + i * bs.AddressSize, bs.AddressSize);

    // hash rewards
    for (size_t i = 0; i < 1; ++i)
        add_hash(data() + bs.offsetReward + i * bs.RewardSize, bs.RewardSize);

    // hash payments
    for (size_t i = 0; i < bs.nTransfers; ++i)
        add_hash(data() + bs.offsetTransfers + i * bs.TransferSize, bs.TransferSize);

    // hash tokens
    foreach_token([&](const TokenSectionView& ts) {
        add_hash(ts.dataBody + ts.ts.beginOffset, 4); // TokenId
        ts.foreach_transfer([&](const TokenTransferView& ttv) {
            add_hash(ttv.data(), ttv.size()); // transfer
        });
        ts.foreach_order([&](const OrderView& ov) {
            add_hash(ov.data(), ov.size()); // order
        });
        ts.foreach_liquidity_add([&](const LiquidityAddView& lav) {
            add_hash(lav.data(), lav.size()); // liquidity add
        });
        ts.foreach_liquidity_remove([&](const LiquidityRemoveView& lrv) {
            add_hash(lrv.data(), lrv.size()); // liquidity remove
        });
    });

    return hashes;
};

std::vector<uint8_t> BodyView::merkle_prefix() const
{
    std::vector<Hash> hashes(merkle_leaves());
    std::vector<Hash> tmp, *from, *to;
    from = &hashes;
    to = &tmp;

    do {
        const size_t I { (from->size() + 1) / 2 };
        to->clear();
        to->reserve(I);
        size_t j = 0;
        for (size_t i = 0; i < I; ++i) {
            if (I == 1) {
                std::vector<uint8_t> res;
                std::copy((*from)[j].begin(), (*from)[j].end(), std::back_inserter(res));
                if (j + 1 < from->size()) {
                    std::copy((*from)[j + 1].begin(), (*from)[j + 1].end(), std::back_inserter(res));
                }
                return res;
            }
            HasherSHA256 hasher {};
            hasher.write((*from)[j].data(), 32);
            if (j + 1 < from->size()) {
                hasher.write((*from)[j + 1].data(), 32);
            }

            to->push_back(std::move(hasher));
            j += 2;
        }
        std::swap(from, to);
    } while (from->size() > 1);
    assert(false);
}

Hash BodyView::merkle_root(Height h) const
{
    std::vector<Hash> hashes(merkle_leaves());
    std::vector<Hash> tmp, *from, *to;
    from = &hashes;
    to = &tmp;

    bool new_root_type = is_testnet() || h.value() >= NEWMERKLEROOT;
    bool block_v2 = is_testnet() || h.value() >= NEWBLOCKSTRUCUTREHEIGHT;
    if (new_root_type) {
        do {
            const size_t I { (from->size() + 1) / 2 };
            to->clear();
            to->reserve(I);
            size_t j = 0;
            for (size_t i = 0; i < I; ++i) {
                HasherSHA256 hasher {};
                hasher.write((*from)[j].data(), 32);
                if (j + 1 < from->size()) {
                    hasher.write((*from)[j + 1].data(), 32);
                }

                if (I == 1)
                    hasher.write(data(), block_v2 ? 10 : 4);
                to->push_back(std::move(hasher));
                j += 2;
            }
            std::swap(from, to);
        } while (from->size() > 1);
        return from->front();
    } else {
        bool includedSeed = false;
        bool finish = false;
        do {
            const size_t I { (from->size() + 1) / 2 };
            to->clear();
            to->reserve(I);
            if (from->size() <= 2 && !includedSeed) {

                HasherSHA256 hasher {};
                hasher.write((*from)[0].data(), 32);
                if (1 < from->size()) {
                    hasher.write((*from)[1].data(), 32);
                }
                hasher.write(data(), 4);
                includedSeed = true;
                to->push_back(std::move(hasher));
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
                    to->push_back(std::move(hasher));
                    j += 2;
                }
            }
            std::swap(from, to);
        } while (!finish);
        return from->front();
    }
}
