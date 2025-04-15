#pragma once
#include "comparators.hpp"
#include "defi/token/account_token.hpp"
#include "general/address_funds.hpp"
#include "mempool/updates.hpp"
#include <set>
namespace chainserver {
struct TransactionIds;
}
namespace mempool {

struct LockedBalance {
    LockedBalance(const Funds_uint64& balance)
        : avail(balance) {};

    void lock(Funds_uint64 amount);
    void unlock(Funds_uint64 amount);
    [[nodiscard]] bool set_avail(Funds_uint64 amount);
    Funds_uint64 remaining() const { return Funds_uint64::diff_assert(avail, used); }
    Funds_uint64 locked() const { return used; }
    bool is_clean() { return used.is_zero(); }

private:
    Funds_uint64 avail { Funds_uint64::zero() };
    Funds_uint64 used { Funds_uint64::zero() };
};

class Mempool {
    using iter_t = Txmap::iterator;
    using const_iter_t = Txmap::const_iterator;

public:
    Mempool(bool master = true, size_t maxSize = 10000)
        : master(master)
        , maxSize(maxSize)
    {
    }

    [[nodiscard]] Updates pop_updates()
    {
        return std::move(updates);
        updates.clear();
    }
    void apply_log(const Updates& log);
    Error insert_tx(const TransferTxExchangeMessage& pm, TransactionHeight txh, const TxHash& hash, const AddressFunds& e);
    void insert_tx_throw(const TransferTxExchangeMessage& pm, TransactionHeight txh, const TxHash& hash, const AddressFunds& e);
    void erase(TransactionId id);
    void set_balance(AccountToken, Funds_uint64 newBalance);
    void erase_from_height(Height);
    void erase_before_height(Height);

    // getters
    [[nodiscard]] auto cache_validity() const { return txs.cache_validity(); }
    [[nodiscard]] auto get_payments(size_t n, NonzeroHeight height, std::vector<Hash>* hashes = nullptr) const
        -> std::vector<TransferTxExchangeMessage>;
    [[nodiscard]] auto sample(size_t) const -> std::vector<TxidWithFee>;
    [[nodiscard]] auto filter_new(const std::vector<TxidWithFee>&) const
        -> std::vector<TransactionId>;

    // operator[]
    [[nodiscard]] auto operator[](const TransactionId& id) const
        -> std::optional<TransferTxExchangeMessage>;
    [[nodiscard]] auto operator[](const HashView txHash) const
        -> std::optional<TransferTxExchangeMessage>;
    [[nodiscard]] size_t size() const { return txs.size(); }
    [[nodiscard]] CompactUInt min_fee() const;

private:
    using BalanceEntries = std::map<AccountToken, LockedBalance>;
    void apply_logevent(const Put&);
    void apply_logevent(const Erase&);
    void erase_internal(Txmap::const_iterator);
    bool erase_internal(Txmap::const_iterator, BalanceEntries::iterator, bool gc = true);
    void prune();

private:
    Updates updates;
    Txmap txs;
    std::set<const_iter_t, ComparatorPin> byPin;
    ByFeeDesc byFee;
    std::set<const_iter_t, ComparatorHash> byHash;
    BalanceEntries lockedBalances;
    bool master;
    size_t maxSize;
};
}
