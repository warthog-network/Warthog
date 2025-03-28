#include "history.hpp"
#include "block/chain/header_chain.hpp"
#include "general/writer.hpp"

auto TransactionVerifier::pin_info(PinNonce pinNonce) const -> PinInfo
{
    PinHeight pinHeight(pinNonce.pin_height_from_floored(pinFloor));
    return PinInfo {
        .height { pinHeight },
        .hash { hc.hash_at(pinHeight) }
    };
}

template <typename... HashArgs>
VerifiedTransaction TransactionVerifier::verify(const SignerData& sd, HashArgs&&... hashArgs) const
{
    const PinFloor pinFloor { h.pin_floor() };
    PinHeight pinHeight(sd.pinNonce.pin_height_from_floored(pinFloor));
    Hash pinHash { hc.hash_at(h) };
    return {
        sd.verify_hash((
            (HasherSHA256()
                << pinHash
                << pinHeight
                << sd.pinNonce.id
                << sd.pinNonce.reserved)
            << ... << std::forward<HashArgs>(hashArgs))),
        { { sd.origin.id, pinHeight, sd.pinNonce.id }, validator }
    };
}

Hash RewardInternal::hash() const
{
    return HasherSHA256()
        << toAddress
        << amount
        << height
        << uint16_t(0);
}

VerifiedOrder::VerifiedOrder(const OrderInternal& o, const TransactionVerifier& verifier, HashView tokenHash)
    : VerifiedTransaction(verifier.verify(
          o,
          o.compactFee.uncompact(),
          o.limit.to_uint32(),
          o.amount.funds,
          tokenHash))
    , order(o)

{
}

VerifiedTokenTransfer::VerifiedTokenTransfer(const TokenTransferInternal& ti, const TransactionVerifier& verifier, HashView tokenHash)
    : VerifiedTransaction(verifier.verify(ti,
          ti.compactFee.uncompact(),
          ti.to.address,
          ti.amount,
          tokenHash))
    , ti(ti)
{
}

VerifiedWartTransfer::VerifiedWartTransfer(const WartTransferInternal& ti, const TransactionVerifier& verifier)
    : VerifiedTransaction(verifier.verify(ti,
          ti.compactFee.uncompact(),
          ti.to.address,
          ti.amount))
    , ti(ti)
{
}

VerifiedTokenCreation::VerifiedTokenCreation(const TokenCreationInternal& tci, const TransactionVerifier& verifier)
    : VerifiedTransaction(verifier.verify(tci)
          // ,tci.tokenName.view() // TODO
          )
    , tci(tci)
{
}

namespace history {

SwapHist::SwapHist(SwapInternal si, bool buyBase, Height h)
    : SwapInternal(std::move(si))
    , hash(HasherSHA256() << uint8_t(buyBase) << si.txid.accountId << base << quote << h)
{
}

Entry::Entry(const RewardInternal& p)
    : hash(p.hash())
{
    data = serialize(RewardData {
        p.toAccountId,
        p.amount });
}

Entry::Entry(const VerifiedWartTransfer& p)
    : hash(p.hash)
{
    data = serialize(WartTransferData {
        p.ti.origin.id,
        p.ti.compactFee,
        p.ti.to.id,
        p.ti.amount,
        p.ti.pinNonce });
}

Entry::Entry(const VerifiedTokenTransfer& p, TokenId tokenId)
    : hash(p.hash)
{
    data = serialize(TokenTransferData {
        tokenId,
        p.txid.accountId,
        p.ti.compactFee,
        p.ti.to.id,
        p.ti.amount,
        p.ti.pinNonce });
}
Entry::Entry(const VerifiedOrder& p)
    : hash(p.hash)
{
    data = serialize(OrderData {
        p.order.amount.tokenId,
        p.order.buy,
        p.txid.accountId,
        p.order.compactFee,
        p.order.limit,
        p.order.amount.funds,
        p.order.pinNonce });
}

Entry::Entry(const VerifiedCancelation& p)
    : hash(p.hash)
{
    // TokenId tokenId;
    // bool buy;
    // AccountId accountId;
    // CompactUInt compactFee;
    data = serialize(CancelationData {
        p.cancelation.pinNonce,
        p.cancelation.tokenId,
        p.txid.accountId,
        p.cancelation.compactFee,
        p.cancelation.limit,
        p.cancelation.amount.funds,
        p.cancelation.pinNonce });
}
TokenTransferData TokenTransferData::parse(Reader& r)
{
    if (r.remaining() != bytesize)
        throw std::runtime_error("Cannot parse TokenTransferData.");
    return TokenTransferData {
        .tokenId { r },
        .fromAccountId { r },
        .compactFee { r },
        .toAccountId { r },
        .amount { Funds_uint64::from_value_throw(r) },
        .pinNonce { r }
    };
}

Entry::Entry(const VerifiedTokenCreation& p)
    : hash(p.hash)
{
    data = serialize(TokenCreationData {
        .creatorAccountId { p.tci.creatorAccountId },
        .pinNonce { p.tci.pinNonce },
        .tokenName { p.tci.tokenName },
        .compactFee { p.tci.compactFee },
        .tokenIndex { p.tokenId } });
}

Entry::Entry(const BuySwapHist& p)
    : hash(p.hash)
    , data(serialize(BuySwapData { p }))
{
}

Entry::Entry(const SellSwapHist& p)
    : hash(p.hash)
    , data(serialize(SellSwapData { p }))
{
}

void TokenTransferData::write(Writer& w) const
{
    assert(w.remaining() == bytesize);
    w << tokenId << fromAccountId << compactFee << toAccountId
      << amount << pinNonce;
}

OrderData OrderData::parse(Reader& r)
{
    return OrderData { r, r, r, r, Price_uint64::from_uint32_throw(r.uint32()), r, r };
}

void OrderData::write(Writer& w) const
{
    assert(w.remaining() == bytesize);
    w << tokenId << buy << accountId << compactFee << amount
      << limit.to_uint32() << pinNonce;
}

void WartTransferData::write(Writer& w) const
{
    assert(w.remaining() == bytesize);
    w << fromAccountId << compactFee << toAccountId
      << amount << pinNonce;
}

WartTransferData WartTransferData::parse(Reader& r)
{
    if (r.remaining() != bytesize)
        throw std::runtime_error("Cannot parse TransferData.");
    return WartTransferData {
        .fromAccountId { r },
        .compactFee { r },
        .toAccountId { r },
        .amount { r },
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
        .toAccountId { r },
        .miningReward { r }
    };
}

void TokenCreationData::write(Writer& w) const
{
    assert(w.remaining() == bytesize);
    w << creatorAccountId << compactFee << pinNonce << tokenName << tokenIndex;
}

TokenCreationData TokenCreationData::parse(Reader& r)
{
    if (r.remaining() != bytesize)
        throw std::runtime_error("Cannot parse TokenCreationData.");
    return TokenCreationData {
        .creatorAccountId { r },
        .pinNonce { r },
        .tokenName { r },
        .compactFee { r },
        .tokenIndex { r },
    };
}
void SwapData::write(Writer& w) const
{
    assert(w.remaining() == bytesize);
    w << oId << accId << base << quote;
}

SwapData SwapData::parse(Reader& r)
{
    if (r.remaining() != bytesize)
        throw std::runtime_error("Cannot parse TokenCreationData.");
    return SwapData { r, r, r, r };
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
