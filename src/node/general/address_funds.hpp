#pragma once
#include "block/body/account_id.hpp"
#include "crypto/address.hpp"
#include "general/funds.hpp"
struct AddressFunds {
    Address address;
    Funds_uint64 funds;
};
struct AddressWart {
    Address address;
    Wart funds;
};
struct AccountWart {
    AccountId accointId;
    Wart funds;
};
