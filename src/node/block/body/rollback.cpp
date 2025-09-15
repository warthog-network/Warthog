#include "rollback.hpp"

BalanceIdFunds::BalanceIdFunds(Reader& r)
    : balanceId(r)
    , funds(r)
{
}
