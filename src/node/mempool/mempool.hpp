#pragma once
#include "block/body/messages.hpp"
#include "comparators.hpp"
#include "general/address_funds.hpp"
#include "mempool/updates.hpp"
namespace chainserver {
struct TransactionIds;
}
namespace mempool {

struct LockedBalance {
    LockedBalance(const Wart& balance)
        : avail(balance) {};

    void lock(Wart amount);
    void unlock(Wart amount);
    [[nodiscard]] bool set_avail(Wart amount);
    Wart remaining() const { return Wart::diff_assert(avail, used); }
    Wart locked() const { return used; }
    bool is_clean() { return used.is_zero(); }

private:
    Wart avail { Wart::zero() };
    Wart used { Wart::zero() };
};

class Mempool {
    using iter_t = Txset::iterator;
    using const_iter_t = Txset::const_iterator;

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
    Error insert_tx(const TransactionMessage& pm, TxHeight txh, const TxHash& hash, const AddressFunds& e);
    void insert_tx_throw(const TransactionMessage& pm, TxHeight txh, const TxHash& hash, const AddressFunds& e);
    void erase(TransactionId id);
    void set_wart_balance(AccountId, Wart newBalance);
    void erase_from_height(Height);
    void erase_before_height(Height);

    // getters
    [[nodiscard]] auto cache_validity() const { return txs.cache_validity(); }
    [[nodiscard]] auto get_transactions(size_t n, NonzeroHeight height, std::vector<Hash>* hashes = nullptr) const
        -> std::vector<TransactionMessage>;
    [[nodiscard]] auto sample(size_t) const -> std::vector<TxidWithFee>;
    [[nodiscard]] auto filter_new(const std::vector<TxidWithFee>&) const
        -> std::vector<TransactionId>;

    // operator[]
    [[nodiscard]] auto operator[](const TransactionId& id) const
        -> std::optional<TransactionMessage>;
    [[nodiscard]] auto operator[](const HashView txHash) const
        -> std::optional<TransactionMessage>;
    [[nodiscard]] size_t size() const { return txs.size(); }
    [[nodiscard]] CompactUInt min_fee() const;

private:
    using BalanceEntries = std::map<AccountId, LockedBalance>;
    void apply_logevent(const Put&);
    void apply_logevent(const Erase&);
    void erase_internal(Txset::const_iterator);
    bool erase_internal(Txset::const_iterator, BalanceEntries::iterator, bool gc = true);
    void prune();

private:
    Updates updates;
    Txset txs;
    std::set<const_iter_t, ComparatorPin> byPin;
    ByFeeDesc byFee;
    std::set<const_iter_t, ComparatorHash> byHash;
    BalanceEntries lockedBalances;
    bool master;
    size_t maxSize;
};
}
