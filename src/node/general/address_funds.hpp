#pragma once
#include "block/body/account_id.hpp"
#include "crypto/address.hpp"
#include "general/funds.hpp"
struct AddressFunds {
    Address address;
    Funds_uint64 funds;
};
struct AccountFunds {
    AccountId accointId;
    Funds_uint64 funds;
};
