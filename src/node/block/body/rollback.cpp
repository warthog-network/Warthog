#include "rollback.cpp"

BalanceIdFunds::BalanceIdFunds(Reader& r)
    : balanceId(r)
    , funds(r)
{
}
