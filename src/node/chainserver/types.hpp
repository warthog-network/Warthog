#pragma once
#include "defi/token/account_token.hpp"
#include "general/funds.hpp"
#include <map>
namespace chainserver {
using free_balance_udpates_t = std::map<AccountToken, Funds_uint64>;
}
