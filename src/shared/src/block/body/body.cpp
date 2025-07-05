#include "body.hpp"
#include "crypto/hasher_sha256.hpp"
#include "general/is_testnet.hpp"
#include "general/writer.hpp"
namespace block {

namespace body {
namespace {

template <size_t N>
struct TenBitsBlocks {
private:
    using arr_t = std::array<size_t, N>;
    static constexpr size_t byte_size() { return (10 * N + 7) / 8; }
    template <size_t... Is>
    static auto read_data(std::index_sequence<Is...>, Reader& rd)
    {
        auto r10 { [bits = size_t(0), nbits = 0u, &rd]() mutable -> size_t {
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
        } };
        return arr_t { (void(Is), r10())... };
    }

public:
    TenBitsBlocks(Reader& rd)
        : data(read_data(std::make_index_sequence<N>(), rd))
    {
    }
    template <typename... Ts>
    requires(sizeof...(Ts) == N)
    TenBitsBlocks(body_vector<Ts>... vs)
        : data { vs.size()... }
    {
    }
    friend Writer& operator<<(Writer& w, const TenBitsBlocks<N>& tbb)
    {
        uint32_t bits { 0 };
        size_t nbits { 0 };
        for (auto v : tbb.data) {
            bits |= uint32_t(v) << (22 - nbits);
            nbits += 10;
            while (nbits >= 8) {
                w << uint8_t(bits >> 24);
                bits <<= 8;
                nbits -= 8;
            }
        }
        if (nbits > 0)
            w << uint8_t(bits >> 24);
        return w;
    }
    template <size_t i>
    requires(i < N)
    size_t at() const
    {
        return data[i];
    }

private:
    arr_t data;
};

}

template <typename T>
size_t body_vector<T>::byte_size() const
{
    size_t i { 0 };
    for (auto& elem : *this) {
        i += elem.byte_size();
    }
    return i;
}
template <typename T>
body_vector<T>::body_vector(size_t n, Reader& r)
{
    for (size_t i { 0 }; i < n; ++i)
        this->push_back({ r });
}

void TokenSection::append_tx_ids(PinFloor pf, std::vector<TransactionId>& appendTo) const
{
    auto append { [&](auto& vec) {
        for (auto& tx : vec) {
            appendTo.push_back(tx.txid_from_floored(pf));
        }
    } };
    append(assetTransfers);
    append(shareTransfers);
    append(orders);
    append(liquidityAdd);
    append(liquidityRemove);
}

size_t TokenSection::byte_size() const
{
    return id.byte_size()
        + assetTransfers.byte_size()
        + shareTransfers.byte_size()
        + orders.byte_size()
        + liquidityAdd.byte_size()
        + liquidityRemove.byte_size();
}

Writer& TokenSection::write(Writer& w)
{
    w << id << TenBitsBlocks<n_vectors>(assetTransfers, shareTransfers, orders, liquidityAdd, liquidityRemove);
    assetTransfers.write_elements(w);
    shareTransfers.write_elements(w);
    orders.write_elements(w);
    liquidityAdd.write_elements(w);
    liquidityRemove.write_elements(w);
    return w;
}

TokenSection::TokenSection(Reader& r)
    : id(r)
{
    TenBitsBlocks<n_vectors> tbb(r);
    assetTransfers = { tbb.at<0>(), r };
    shareTransfers = { tbb.at<1>(), r };
    orders = { tbb.at<2>(), r };
    liquidityAdd = { tbb.at<3>(), r };
    liquidityRemove = { tbb.at<4>(), r };
}

}

std::vector<TransactionId> Body::tx_ids(NonzeroHeight height) const
{
    auto pf { height.pin_floor() };
    std::vector<TransactionId> res;
    for (auto& t : wartTransfers)
        res.push_back(t.txid_from_floored(pf));
    for (auto& t : tokens)
        t.append_tx_ids(pf, res);
    return res;
}

Body Body::parse_throw(std::span<const uint8_t> data, NonzeroHeight h, BlockVersion version)
{
    Reader rd(data);
    try {
        bool block_v4 { version.value() >= 4 };
        bool block_v2 = is_testnet() || h.value() >= NEWBLOCKSTRUCUTREHEIGHT;

        if (rd.remaining() > MAXBLOCKSIZE)
            throw Error(EINV_BODY);
        if (block_v2) {
            // Read new address section
            rd.skip(10); // for mining
            body_vector<Address> addresses(rd.uint16(), rd);
            body::Reward reward { rd };
            Body b { std::move(addresses), std::move(reward) };
            if (rd.remaining() > 0) {
                b.wartTransfers = { rd.uint32(), rd };

                if (block_v4) {
                    b.cancelations = { rd.uint16(), rd };
                    b.tokens = { rd.uint16(), rd };
                }
            }
            return b;
        } else {
            // Read new address section
            rd.skip(4); // for mining
            body_vector<Address> addresses { rd.uint32(), rd };

            // Read reward section
            rd.skip(2);
            body::Reward reward { rd };
            Body b { std::move(addresses), std::move(reward) };

            // Read WART transfer section
            b.wartTransfers = { rd.uint32(), rd };
            return b;
        }
    } catch (const Error& e) {
        if (e.code == EMSGINTEGRITY)
            throw Error(EINV_BODY); // more meaningful error
        throw e;
    }
}

size_t Body::byte_size() const
{
    size_t res { 0 };
    res += 10; // for mining
    res += 2 + newAddresses.byte_size();
    res += reward.byte_size();
    res += 4 + wartTransfers.byte_size();
    res += 2 + cancelations.byte_size();
    res += 2 + tokens.byte_size();

    return res;
}

std::vector<uint8_t> Body::serialize() const
{
    std::vector<uint8_t> res(byte_size(), 0);
    Writer w(res);
    w.skip(10); // for mining
    newAddresses.write<uint16_t>(w);
    w << reward;
    wartTransfers.write<uint32_t>(w);
    cancelations.write<uint16_t>(w);
    tokens.write<uint16_t>(w);
    assert(w.remaining() == 0);
    return res;
};

Body::Body(std::span<const uint8_t> data, BlockVersion v, NonzeroHeight h)
    : Body(parse_throw(data, h, v)) {
    };

std::vector<Hash> Body::merkle_leaves() const
{
    std::vector<Hash> hashes;
    auto add_hash { [&](const auto& s) {
        hashes.push_back(hashSHA256(s));
    } };
    auto add_vec_hashes { [&](const auto& v) {
        for (auto& e : v)
            add_hash(e);
    } };

    add_vec_hashes(newAddresses);
    add_hash(reward);
    add_vec_hashes(cancelations);
    for (auto& t : tokens) {
        static_assert(body::TokenSection::n_vectors == 5);
        add_vec_hashes(t.assetTransfers);
        add_vec_hashes(t.shareTransfers);
        add_vec_hashes(t.orders);
        add_vec_hashes(t.liquidityAdd);
        add_vec_hashes(t.liquidityRemove);
    }
    add_vec_hashes(assetCreations);

    return hashes;
};

std::vector<uint8_t> Body::merkle_prefix() const
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
                if (j + 1 < from->size())
                    std::copy((*from)[j + 1].begin(), (*from)[j + 1].end(), std::back_inserter(res));
                return res;
            }
            HasherSHA256 hasher {};
            hasher.write((*from)[j]);
            if (j + 1 < from->size()) {
                hasher.write((*from)[j + 1]);
            }

            to->push_back(std::move(hasher));
            j += 2;
        }
        std::swap(from, to);
    } while (from->size() > 1);
    assert(false);
}

Hash Body::merkle_root(Height h) const
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
                hasher.write((*from)[j]);
                if (j + 1 < from->size()) {
                    hasher.write((*from)[j + 1]);
                }

                if (I == 1)
                    hasher.write({ data(), block_v2 ? 10 : 4 });
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
                hasher.write((*from)[0]);
                if (1 < from->size()) {
                    hasher.write((*from)[1]);
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
                    hasher.write((*from)[j]);
                    if (j + 1 < from->size()) {
                        hasher.write((*from)[j + 1]);
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
