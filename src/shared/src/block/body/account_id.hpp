#pragma once
#include "crypto/address.hpp"
#include "general/errors.hpp"
#include "general/with_uint64.hpp"

class ValidAccountId;

class AccountId : public UInt64WithOperators<AccountId> { // NOTE: >=0 is important for AccountHeights
public:
    bool operator==(const AccountId&) const = default;
    using parent_t::parent_t;
    [[nodiscard]] ValidAccountId validate_throw(AccountId beginInvalid) const;
};

class BalanceId : public UInt64WithOperators<BalanceId> {
public:
    bool operator==(const BalanceId&) const = default;
    using parent_t::parent_t;
};

class ValidAccountId : public AccountId {
private:
    friend class AccountId;
    ValidAccountId(AccountId id)
        : AccountId(std::move(id))
    {
    }
};

inline ValidAccountId AccountId::validate_throw(AccountId beginInvalid) const
{
    if (*this < beginInvalid)
        return *this;
    throw Error(EIDPOLICY);
}

struct IdAddressView {
    ValidAccountId id;
    AddressView address;
};

struct IdAddress {
    AccountId id;
    Address address;
    IdAddress(const IdAddressView& v)
        : id(v.id)
        , address(v.address)
    {
    }
};
