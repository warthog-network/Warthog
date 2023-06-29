#pragma once
#include "block/body/account_id.hpp"
#include "general/funds.hpp"
#include "general/reader.hpp"

class RollbackView {
public:
    class AccountBalance {
        friend class RollbackView;
        AccountBalance(const uint8_t* pos)
            : pos(pos)
        {
        }

    public:
        AccountId id() { return AccountId(readuint64(pos)); }
        Funds balance() { return Funds(readuint64(pos + 8)); }

    private:
        const uint8_t* pos;
    };
    RollbackView(std::vector<uint8_t>& bytes)
        : bytes(bytes)
    {
        if ((bytes.size() % 16) != 8) {
            throw std::runtime_error("Database corrupted (invalid rollback data)");
        }
    };
    AccountId getBeginNewAccounts() { return AccountId(readuint64(bytes.data())); }
    size_t nAccounts() { return bytes.size() >> 4; } //=(size-8)/16;
    AccountBalance accountBalance(size_t i) { return bytes.data() + 8 + i * 16; }

private:
    std::vector<uint8_t>& bytes;
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
    AccountId beginNewAccounts;
};
