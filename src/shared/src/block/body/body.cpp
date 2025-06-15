#include "body.hpp"
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
    append(transfers);
    append(orders);
    append(liquidityAdd);
    append(liquidityRemove);
}

size_t TokenSection::byte_size() const
{
    return id.byte_size()
        + transfers.byte_size()
        + orders.byte_size()
        + liquidityAdd.byte_size()
        + liquidityRemove.byte_size();
}

Writer& TokenSection::write(Writer& w)
{
    w << id << TenBitsBlocks<n_vectors>(transfers, orders, liquidityAdd, liquidityRemove);
    return transfers.write_elements(w);
}

TokenSection::TokenSection(Reader& r)
    : id(r)
{
    TenBitsBlocks<n_vectors> tbb(r);
    transfers = { tbb.at<0>(), r };
    orders = { tbb.at<1>(), r };
    liquidityAdd = { tbb.at<2>(), r };
    liquidityRemove = { tbb.at<3>(), r };
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

                    // // read new token section
                    // if (defiEnabled && rd.remaining() != 0) {
                    //     bs.nNewTokens = rd.uint8();
                    // }
                    // bs.offsetNewTokens = rd.offset();
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
}
