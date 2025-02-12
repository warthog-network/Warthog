#include "view.hpp"
#include "block/body/parse.hpp"
#include "crypto/crypto.hpp"
#include "crypto/hasher_sha256.hpp"
#include "defi/token/id.hpp"
#include "general/hex.hpp"
#include "general/is_testnet.hpp"
#include "general/reader.hpp"
#include <iostream>
#include <vector>
using namespace std;

BodyView::BodyView(std::span<const uint8_t> s, NonzeroHeight h)
    : s(s)
{
    static_assert(SIGLEN == RecoverableSignature::length);
    Reader rd { s };
    bool block_v3 = is_testnet() || h.value() >= TOKENSTARTHEIGHT;
    bool block_v2 = is_testnet() || h.value() >= NEWBLOCKSTRUCUTREHEIGHT;

    if (s.size() > MAXBLOCKSIZE)
        return;
    const bool defiEnabled { true };
    if (block_v2) {
        // Read new address section
        rd.skip(10); // for mining
        nAddresses = rd.uint16();
        offsetAddresses = rd.cursor() - s.data();
        if (rd.remaining() < nAddresses * AddressSize)
            return;
        rd.skip(nAddresses * AddressSize);

        // Read reward section
        if (rd.remaining() < RewardSize)
            return;
        offsetReward = rd.cursor() - s.data();
        rd.skip(16);

        // Read payment section
        if (rd.remaining() != 0) {
            nTransfers = rd.uint32();
            // Make sure that it has correct length
            if (rd.remaining() < (TransferSize)*nTransfers)
                return;
        }
        offsetTransfers = rd.cursor() - s.data();
        rd.skip(nTransfers * TransferSize);

        if (block_v3) {
            // read token sections
            nTokens = rd.uint16();
            for (size_t i = 0; i < nTokens; ++i) {
                // TokenSection{
                //     .nTransfers {rd.uint16()},
                //     .transfersBegin {rd.cursor()};
                //
                // }
                auto begin { rd.cursor() };
                TokenId tokenId { rd.uint32() };

                // 5 bytes for 4 10-bit length specifications
                size_t nTransfers { rd.uint8() };
                size_t nOrders { rd.uint8() };
                size_t nLiquidityAdd { rd.uint8() };
                size_t nLiquidityRemove { rd.uint8() };
                size_t extend { rd.uint8() };
                nTransfers += ((extend >> 6) & 0x03) << 8;
                nOrders += ((extend >> 4) & 0x03) << 8;
                nLiquidityAdd += ((extend >> 2) & 0x03) << 8;
                nLiquidityRemove += ((extend)&0x03) << 8;

                auto transfersBegin { rd.cursor() };
                rd.skip(TransferSize * nTransfers);
                auto ordersBegin { rd.cursor() };
                rd.skip(OrderSize * nOrders);
                auto liquidityAddBegin { rd.cursor() };
                rd.skip(LiquidityAddSize * nLiquidityAdd);
                auto liquidityRemoveBegin { rd.cursor() };
                rd.skip(LiquidityRemoveSize * nLiquidityRemove);

                tokensSections.push_back(
                    TokenSection {
                        .begin = begin,
                        .tokenId = tokenId,
                        .nTransfers = nTransfers,
                        .transfersBegin = transfersBegin,
                        .nOrders = nOrders,
                        .ordersBegin = ordersBegin,
                        .nLiquidityAdd = nLiquidityAdd,
                        .liquidityAddBegin = liquidityAddBegin,
                        .nLiquidityRemove = nLiquidityRemove,
                        .liquidityRemoveBegin = liquidityRemoveBegin });
            }

            // read new token section
            if (defiEnabled && rd.remaining() != 0) {
                nNewTokens = rd.uint8();
            }
            offsetNewTokens = rd.cursor() - s.data();
        }
    } else {
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
        rd.skip(2);
        if (rd.remaining() < RewardSize + 4)
            return;
        offsetReward = rd.cursor() - s.data();
        rd.skip(16);

        // Read payment section
        nTransfers = rd.uint32();
        // Make sure that it has correct length
        if (rd.remaining() != (TransferSize)*nTransfers)
            return;
        offsetTransfers = rd.cursor() - s.data();
    }
    isValid = true;
};

std::vector<Hash> BodyView::merkle_leaves() const
{
    std::vector<Hash> hashes(nAddresses + 1 + nTransfers + nTokens);

    // hash addresses
    size_t idx = 0;
    for (size_t i = 0; i < nAddresses; ++i)
        hashes[idx++] = hashSHA256(s.data() + offsetAddresses + i * AddressSize, AddressSize);

    // hash rewards
    for (size_t i = 0; i < 1; ++i)
        hashes[idx++] = hashSHA256(data() + offsetReward + i * RewardSize, RewardSize);
    // hash payments
    for (size_t i = 0; i < nTransfers; ++i)
        hashes[idx++] = hashSHA256(data() + offsetTransfers + i * TransferSize, TransferSize);
    foreach_token([&](const TokenSection& ts) {
        hashes[idx++] = hashSHA256(ts.begin, 4); // TokenId
        ts.foreach_transfer([&](const TokenTransferView& ttv) {
            hashes[idx++] = hashSHA256(ttv.data(), ttv.size()); // transfer
        });
        ts.foreach_order([&](const OrderView& ov) {
            hashes[idx++] = hashSHA256(ov.data(), ov.size()); // order
        });
        ts.foreach_liquidity_add([&](const LiquidityAddView& lav) {
            hashes[idx++] = hashSHA256(lav.data(), lav.size()); // liquidity add
        });
        ts.foreach_liquidity_remove([&](const LiquidityRemoveView& lrv) {
            hashes[idx++] = hashSHA256(lrv.data(), lrv.size()); // liquidity remove
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
        to->resize((from->size() + 1) / 2);
        size_t j = 0;
        for (size_t i = 0; i < (from->size() + 1) / 2; ++i) {
            if (to->size() == 1) {
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

            (*to)[i] = std::move(hasher);
            j += 2;
        }
        std::swap(from, to);
    } while (from->size() > 1);
    assert(false);
}

Hash BodyView::merkle_root(Height h) const
{
    assert(isValid);
    std::vector<Hash> hashes(merkle_leaves());
    std::vector<Hash> tmp, *from, *to;
    from = &hashes;
    to = &tmp;

    bool new_root_type = is_testnet() || h.value() >= NEWMERKLEROOT;
    bool block_v2 = is_testnet() || h.value() >= NEWBLOCKSTRUCUTREHEIGHT;
    if (new_root_type) {
        do {
            to->resize((from->size() + 1) / 2);
            size_t j = 0;
            for (size_t i = 0; i < (from->size() + 1) / 2; ++i) {
                HasherSHA256 hasher {};
                hasher.write((*from)[j].data(), 32);
                if (j + 1 < from->size()) {
                    hasher.write((*from)[j + 1].data(), 32);
                }

                if (to->size() == 1)
                    hasher.write(data(), block_v2 ? 10 : 4);
                (*to)[i] = std::move(hasher);
                j += 2;
            }
            std::swap(from, to);
        } while (from->size() > 1);
        return from->front();
    } else {
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
}
