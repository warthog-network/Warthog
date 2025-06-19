#include "types.hpp"
// ProcessedMatch::ProcessedMatch(const Hash& blockHash, TokenId tokenId){
//     , hash(HasherSHA256() << uint8_t(buyBase) << si.txid.accountId << base << quote << h)
// {
// }

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
VerifiedCancelation::VerifiedCancelation(const CancelationInternal& c, const TransactionVerifier& verifier)
    : VerifiedTransaction(verifier.verify(
          c,
          c.compactFee.uncompact(),
          c.cancelTxid))
    , cancelation(c)
{
}

VerifiedLiquidityDeposit::VerifiedLiquidityDeposit(const LiquidityDepositsInternal& l, const TransactionVerifier& verifier)
    : VerifiedTransaction(verifier.verify(l, l.basequote))
    , liquidityAdd { l }
{
}
VerifiedLiquidityWithdrawal::VerifiedLiquidityWithdrawal(const LiquidityWithdrawalInternal& l, const TransactionVerifier& verifier)
    : VerifiedTransaction(verifier.verify(l, l.poolShares))
    , liquidityAdd(l)
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
    : VerifiedTransaction(verifier.verify(tci,
          tci.supply.funds,
          tci.supply.precision.value(),
          tci.name.view()))
    , tci(tci)
{
}
