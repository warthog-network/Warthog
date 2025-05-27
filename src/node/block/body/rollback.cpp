#include "rollback.hpp"

Writer& operator<<(Writer& w, const BalanceIdFunds& bif)
{
    return w << bif.balanceId << bif.funds;
}

BalanceIdFunds::BalanceIdFunds(Reader& r)
    : balanceId(r)
    , funds(r)
{
}
