#pragma once
#include "comparators.hpp"
#include "general/address_funds.hpp"
#include "mempool/log.hpp"
#include <set>
#include <vector>
namespace chainserver{
    struct TransactionIds;
}
namespace mempool {
struct BalanceEntry {
    friend class Mempool;
    Funds avail { 0 };
    Address address;
    Funds used() { return _used; };
    BalanceEntry(const AddressFunds& af)
        : avail(af.funds)
        , address(af.address) {};
private:
    Funds _used { 0 };
};

class Mempool {
    using iter_t = Txmap::iterator;

public:
    Mempool(bool master = true, size_t maxSize = 100000)
        : master(master)
        , maxSize(maxSize)
    {
    }

    [[nodiscard]] Log pop_log()
    {
        return std::move(log);
        log.clear();
    }
    void apply_log(const Log& log);
    int32_t insert_tx(const TransferTxExchangeMessage& pm, TransactionHeight txh, const TxHash& hash, const AddressFunds& e);
    void erase(TransactionId id);
    void erase_from_height(Height);
    void erase_before_height(Height);

    // getters
    [[nodiscard]] auto get_payments(size_t n, std::vector<Hash>* hashes = nullptr) const
        -> std::vector<TransferTxExchangeMessage>;
    [[nodiscard]] auto take(size_t) const -> std::vector<TxidWithFee>;
    [[nodiscard]] auto filter_new(const std::vector<TxidWithFee>&) const
        -> std::vector<TransactionId>;

    // operator[]
    [[nodiscard]] auto operator[](const TransactionId& id) const
        -> std::optional<TransferTxExchangeMessage>;
    [[nodiscard]] auto operator[](const HashView txHash) const
        -> std::optional<TransferTxExchangeMessage>;

private:
    void apply_logevent(const Put&);
    void apply_logevent(const Erase&);
    void erase(Txmap::iterator);

private:

    Log log;
    Txmap txs;
    std::set<iter_t, ComparatorPin> byPin;
    std::set<iter_t, ComparatorFee> byFee;
    std::set<iter_t, ComparatorHash> byHash;
    std::map<AccountId, BalanceEntry> balanceEntries;
    bool master;
    size_t maxSize;
};
}
