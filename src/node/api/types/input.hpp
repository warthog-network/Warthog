#pragma once

#include "block/body/account_id.hpp"
#include "crypto/address.hpp"
#include "crypto/hash.hpp"
#include "defi/token/id.hpp"
#include "tools/alternative.hpp"
class Reader;

namespace api {
struct TokenIdOrHash : public wrt::alternative<TokenId, AssetHash> {
    using alternative::alternative;
};
struct AccountIdOrAddress : public wrt::alternative<AccountId, Address> {
    using alternative::alternative;
};
}
