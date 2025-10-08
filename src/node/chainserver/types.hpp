#pragma once
#include "block/body/account_id.hpp"
#include "defi/token/id.hpp"
#include "general/funds.hpp"
#include <map>
namespace chainserver {
using free_balance_udpates_t = std::map<AccountId, std::map<TokenId, Funds_uint64>>;
}
