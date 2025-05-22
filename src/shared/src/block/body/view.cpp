#include "view.hpp"
#include "crypto/hasher_sha256.hpp"
#include "defi/token/id.hpp"
#include "general/is_testnet.hpp"
#include "general/reader.hpp"
#include <vector>
using namespace std;

namespace block {
namespace body {
template <typename TransactionView>
Section<TransactionView>::Section(size_t n, Reader& r)
    : n(n)
    , offset(r.offset())
{
    r.skip(n * TransactionView::size());
}

Structure Structure::parse_throw(std::span<const uint8_t> s, NonzeroHeight h, BlockVersion version)
{
    try {
        Structure bs;
        Reader rd { s };
        bool block_v4 { version.value() >= 4 };
        bool block_v2 = is_testnet() || h.value() >= NEWBLOCKSTRUCUTREHEIGHT;

        if (s.size() > MAXBLOCKSIZE)
            throw Error(EINV_BODY);
        const bool defiEnabled { true };
        if (block_v2) {
            // Read new address section
            rd.skip(10); // for mining
            bs.addresses = { rd.uint16(), rd };
            bs.reward = rd.view<view::Reward>();
            if (rd.remaining() == 0)
                return bs;

            bs.wartTransfers = { rd.uint32(), rd };

            if (block_v4) {
                // read token sections
                size_t nTokens { rd.uint16() };
                for (size_t i = 0; i < nTokens; ++i) {
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

                    bs.tokens.push_back(
                        TokenSection {
                            .id { tokenId },
                            .transfers { nTransfers, rd },
                            .orders { nOrders, rd },
                            .liquidityAdd { nLiquidityAdd, rd },
                            .liquidityRemove { nLiquidityRemove, rd },
                            .cancelations { nCancelations, rd },
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
            rd.skip(4); // for mining
            bs.addresses = { rd.uint32(), rd };

            // Read reward section
            rd.skip(2);
            bs.reward = rd.view<view::Reward>();
            rd.skip(16);

            // Read WART transfer section
            bs.wartTransfers = { rd.uint32(), rd };
        }
        return bs;
    } catch (const Error& e) {
        if (e.code == EMSGINTEGRITY)
            throw Error(EINV_BODY); // more meaningful error
        throw e;
    }
}

std::vector<Hash> BodyView::merkle_leaves() const
{
    auto& bs { structure };
    std::vector<Hash> hashes;
    auto add_hash { [&](const auto& s) {
        hashes.push_back(hashSHA256(s));
    } };

    for (auto a : addresses())
        add_hash(a);
    add_hash(bs.reward);
    for (auto t : wart_transfers())
        add_hash(t);

    for (auto t : tokens()) {
        add_hash(t.id());
        for (auto c : t.transfers())
            add_hash(c);
        for (auto c : t.orders())
            add_hash(c);
        for (auto c : t.liquidityAdd())
            add_hash(c);
        for (auto c : t.liquidityRemove())
            add_hash(c);
        for (auto c : t.cancelations())
            add_hash(c);
    }

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
}
}
