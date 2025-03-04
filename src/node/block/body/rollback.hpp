#pragma once
#include "block/body/account_id.hpp"
#include "defi/token/account_token.hpp"
#include "general/funds.hpp"
#include "general/reader.hpp"
#include "general/writer.hpp"

struct BalanceIdFunds {
    BalanceId id;
    Funds balance;
};

class RollbackView {
public:
    RollbackView(const std::vector<uint8_t>& bytes)
        : bytes(bytes)
    {
        if (bytes.size() < 20 || ((bytes.size() - 20) % 16) != 0) {
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
                BalanceIdFunds {
                    .id { readuint64(pos) },
                    .balance { Funds::from_value_throw(readuint64(pos + 8)) } });
            pos += 16;
        }
    }

private:
    const std::vector<uint8_t>& bytes;
};

class RollbackGenerator {
public:
    RollbackGenerator(AccountId beginNewAccounts, BalanceId beginNewBalance, TokenId beginNewTokens)
        : beginNewAccounts(beginNewAccounts)
        , beginNewBalance(beginNewBalance)
        , beginNewTokens(beginNewTokens)
    {
    }
    void register_balance(BalanceId balanceId, Funds originalBalance)
    {
        if (balanceId >= beginNewBalance)
            return;
        assert(originalBalances.insert_or_assign(balanceId, originalBalance).second);
    }
    std::vector<uint8_t> serialze()
    {

        static_assert(std::is_base_of_v<IsUint64, decltype(beginNewAccounts)>);
        static_assert(std::is_base_of_v<IsUint64, decltype(beginNewBalance)>);
        static_assert(std::is_base_of_v<IsUint32, decltype(beginNewTokens)>); // 4 bytes
        constexpr size_t size_per_balance {
            BalanceId::byte_size()
            + Funds::byte_size()
        };
        size_t bytesize = beginNewAccounts.byte_size()
            + beginNewBalance.byte_size()
            + beginNewTokens.byte_size()
            + size_per_balance * originalBalances.size();
        std::vector<uint8_t> res(bytesize);
        Writer w(res.data(), res.size());
        w << beginNewAccounts << beginNewBalance << beginNewTokens;
        for (auto& [balanceId, balance] : originalBalances) {
            w << balanceId << balance;
        }
        return res;
    }

    AccountId begin_new_accounts() const { return beginNewAccounts; };

private:
    std::map<BalanceId, Funds> originalBalances;
    const AccountId beginNewAccounts;
    const BalanceId beginNewBalance;
    const TokenId beginNewTokens;
};
