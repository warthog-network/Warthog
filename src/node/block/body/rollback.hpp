#pragma once
#include "block/body/account_id.hpp"
#include "defi/token/account_token.hpp"
#include "general/funds.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"
#include <variant>

struct IdBalance {
    BalanceId id;
    Funds balance;
};

class RollbackViewV1 {
public:
    RollbackViewV1(const std::vector<uint8_t>& bytes)
        : bytes(bytes)
    {
        if (bytes.size() < 8 || ((bytes.size() - 8) % 16) != 0) {
            throw std::runtime_error("Database corrupted (invalid rollback data)");
        }
    };
    AccountId getBeginNewAccounts() const { return AccountId(readuint64(bytes.data())); }
    TokenId getBeginNewTokens() const { return TokenId(0); }
    BalanceId getBeginAccountTokens() const { return BalanceId(0); }

    void foreach_balance_update(const auto& lambda) const
    {
        auto pos { bytes.data() + 8 };
        auto end { bytes.data() + bytes.size() };
        while (pos < end) {
            lambda(
                IdBalance {
                    .id { readuint64(pos) },
                    .balance { Funds::from_value_throw(readuint64(pos + 8)) } });
            pos += 16;
        }
    }

private:
    const std::vector<uint8_t>& bytes;
};

class RollbackViewV2 {
public:
    RollbackViewV2(const std::vector<uint8_t>& bytes)
        : bytes(bytes)
    {
        if (bytes.size() < 20 || (bytes.size() % 20) != 0) {
            throw std::runtime_error("Database corrupted (invalid rollback data)");
        }
    };
    AccountId getBeginNewAccounts() const { return AccountId(readuint64(bytes.data())); }
    BalanceId getBeginAccountTokens() const { return BalanceId(readuint64(bytes.data() + 8)); }
    TokenId getBeginNewTokens() const { return TokenId(readuint32(bytes.data() + 16)); }
    void foreach_balance_update(const auto& lambda) const
    {
        auto pos { bytes.data() + 20 };
        auto end { bytes.data() + bytes.size() };
        while (pos < end) {
            lambda(
                IdBalance {
                    .id { readuint64(pos) },
                    .balance { Funds::from_value_throw(readuint64(pos + 8)) } });
            pos += 16;
        }
    }

private:
    const std::vector<uint8_t>& bytes;
};

class RollbackView {
    using variant_t = std::variant<RollbackViewV1, RollbackViewV2>;
    auto visit(auto lambda) const
    {
        return std::visit(lambda, variant);
    }
    variant_t initialize(const std::vector<uint8_t>& bytes, bool v1)
    {
        if (v1) {
            return RollbackViewV1(bytes);
        }
        return RollbackViewV2(bytes);
    }

public:
    RollbackView(const std::vector<uint8_t>& bytes, bool v1)
        : variant(initialize(bytes, v1))
    {
    }

    void foreach_balance_update(const auto& lambda) const
    {
        visit([&](auto& v) { return v.foreach_balance_update(lambda); });
    }
    AccountId getBeginNewAccounts() const
    {
        return visit([&](auto& v) {
            return v.getBeginNewAccounts();
        });
    }
    BalanceId getBeginTokenBalance() const
    {
        return visit([&](auto& v) {
            return v.getBeginAccountTokens();
        });
    }
    TokenId getBeginNewTokens() const
    {
        return visit([&](auto& v) {
            return v.getBeginNewTokens();
        });
    }

private:
    variant_t variant;
};

class RollbackGenerator {
public:
    RollbackGenerator(AccountId beginNewAccounts)
        : beginNewAccounts(beginNewAccounts)
    {
    }
    void register_balance(AccountId accountId, Funds originalBalance)
    {
        if (accountId >= beginNewAccounts)
            return;
        assert(originalBalances.insert_or_assign(accountId, originalBalance).second);
        ;
    }
    std::vector<uint8_t> serialze()
    {
        size_t bytesize = 8 + 16 * originalBalances.size();
        std::vector<uint8_t> res(bytesize);
        Writer w(res.data(), res.size());
        w << beginNewAccounts;
        for (auto p : originalBalances) {
            AccountId accountId = p.first;
            Funds balance = p.second;
            w << accountId << balance;
        }
        return res;
    }

    AccountId begin_new_accounts() const { return beginNewAccounts; };

private:
    std::map<AccountId, Funds> originalBalances;
    const AccountId beginNewAccounts;
};
