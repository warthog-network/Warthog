#pragma once
#include "general/errors.hpp"
#include "general/with_uint64.hpp"
#include <compare>
#include <cstddef>
#include <cstdint>

class ValidAccountId;
class AccountId : public IsUint64 { // NOTE: >=0 is important for AccountHeights
public:
    using IsUint64::IsUint64;
    bool operator==(const AccountId&) const = default;
    size_t operator-(AccountId a)
    {
        return val - a.val;
    }
    AccountId operator-(size_t i) const
    {
        return AccountId(val - i);
    }
    AccountId operator+(size_t i) const
    {
        return AccountId(val + i);
    }
    AccountId operator++(int)
    {
        return AccountId(val++);
    }

    [[nodiscard]] ValidAccountId validate_throw(AccountId beginInvalid) const;
};

class BalanceId : public IsUint64 { // NOTE: >=0 is important for BalanceId
public:
    using IsUint64::IsUint64;
    bool operator==(const BalanceId&) const = default;
    size_t operator-(BalanceId a)
    {
        return val - a.val;
    }
    BalanceId operator-(size_t i) const
    {
        return BalanceId(val - i);
    }
    BalanceId operator+(size_t i) const
    {
        return BalanceId(val + i);
    }
    BalanceId operator++(int)
    {
        return BalanceId(val++);
    }
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
