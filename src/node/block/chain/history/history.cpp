#include "history.hpp"
#include "block/chain/header_chain.hpp"

VerifiedTransfer TransferInternal::verify(const Headerchain& hc, NonzeroHeight height) const
{
    assert(height <= hc.length() + 1);
    assert(!fromAddress.is_null());
    assert(!toAddress.is_null());
    const PinFloor pinFloor { PrevHeight(height) };
    PinHeight pinHeight(pinNonce.pin_height(pinFloor));
    Hash pinHash { hc.hash_at(pinHeight) };
    return VerifiedTransfer(*this, pinHeight, pinHash);
}

Hash RewardInternal::hash() const
{
    return HasherSHA256()
        << toAddress
        << amount
        << height
        << offset;
}

bool VerifiedTransfer::valid_signature() const
{
    assert(!ti.fromAddress.is_null());
    assert(!ti.toAddress.is_null());
    auto recovered = recover_address();
    return recovered == ti.fromAddress;
}
VerifiedTransfer::VerifiedTransfer(const TransferInternal& ti, PinHeight pinHeight, HashView pinHash)
    : ti(ti)
    , id { ti.fromAccountId, pinHeight, ti.pinNonce.id }
    , hash(HasherSHA256()
          << pinHash
          << pinHeight
          << ti.pinNonce.id
          << ti.pinNonce.reserved
          << ti.compactFee.uncompact()
          << ti.toAddress
          << ti.amount)
{
    if (!valid_signature())
        throw Error(ECORRUPTEDSIG);
}

namespace history {
Entry::Entry(const RewardInternal& p)
{
    data = serialize(RewardData {
        p.toAccountId,
        p.amount });
    hash = p.hash();
}

Entry::Entry(const VerifiedTransfer& p)
{
    data = serialize(TransferData {
        p.ti.fromAccountId,
        p.ti.compactFee,
        p.ti.toAccountId,
        p.ti.amount,
        p.ti.pinNonce });
    hash = p.hash;
}

void TransferData::write(Writer& w) const
{
    assert(w.remaining() == bytesize);
    w << fromAccountId << compactFee << toAccountId
      << amount << pinNonce;
}

TransferData TransferData::parse(Reader& r)
{
    if (r.remaining() != bytesize)
        throw std::runtime_error("Cannot parse TransferData.");
    return TransferData {
        .fromAccountId { r },
        .compactFee { CompactUInt::from_value_throw(r) },
        .toAccountId { r },
        .amount { Funds::from_value_throw(r) },
        .pinNonce { r }
    };
}

void RewardData::write(Writer& w) const
{
    assert(w.remaining() == bytesize);
    w << toAccountId << miningReward;
}

RewardData RewardData::parse(Reader& r)
{
    if (r.remaining() != bytesize)
        throw std::runtime_error("Cannot parse RewardData.");
    return RewardData {
        .toAccountId = AccountId(r.uint64()),
        .miningReward = Funds::from_value_throw(r.uint64())
    };
}

std::vector<uint8_t> serialize(const Data& entry)
{
    return std::visit([](auto& e) {
        std::vector<uint8_t> data(1 + e.bytesize);
        Writer w(data);
        w << e.indicator;
        e.write(w);
        return data;
    },
        entry);
}

// do metaprogramming dance
//
template <typename V, uint8_t prevcode>
V check(uint8_t, Reader&)
{
    throw std::runtime_error("Cannot parse history entry, unknown variant type");
}

template <typename V, uint8_t prevIndicator, typename T, typename... S>
V check(uint8_t indicator, Reader& r)
{
    // variant indicators must be all different and in order
    static_assert(prevIndicator < T::indicator);
    if (T::indicator == indicator) 
        return T::parse(r);
    return check<V, T::indicator, S...>(indicator, r);
}

template <typename Variant, typename T, typename... S>
Variant check_first(uint8_t indicator, Reader& r)
{
    if (T::indicator == indicator) 
        return T::parse(r);
    return check<Variant, T::indicator, S...>(indicator, r);
}

template <typename... Types>
auto parse_recursive(Reader& r)
{
    using Variant = std::variant<Types...>;
    auto indicator { r.uint8() };
    return check_first<Variant, Types...>(indicator, r);
}

template <typename T>
class VariantParser {
};

template <typename... Types>
struct VariantParser<std::variant<Types...>> {
    static auto parse(std::vector<uint8_t>& v)
    {
        Reader r(v);
        auto res { parse_recursive<Types...>(r) };
        if (!r.eof())
            throw Error(EMSGINTEGRITY);
        return res;
    }
};

Data parse_throw(std::vector<uint8_t> v)
{
    return VariantParser<Data>::parse(v);
}
}
