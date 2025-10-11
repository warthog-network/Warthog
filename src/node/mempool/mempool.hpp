#pragma once
#include "block/body/messages.hpp"
#include "chainserver/state/helpers/cache_fwd.hpp"
#include "comparators.hpp"
#include "defi/token/account_token.hpp"
#include "mempool/updates.hpp"
namespace chainserver {
struct TransactionIds;
}
namespace mempool {

struct LockedBalance {
    LockedBalance(Funds_uint64 total)
        : avail(std::move(total)) { };

    void lock(Funds_uint64 amount);
    void unlock(Funds_uint64 amount);
    [[nodiscard]] bool set_avail(Funds_uint64 amount);
    auto free() const { return Funds_uint64::diff_assert(avail, used); }
    auto locked() const { return used; }
    auto total() const { return Funds_uint64::sum_assert(avail, used); }
    bool is_clean() { return used.is_zero(); }

private:
    Funds_uint64 avail { Funds_uint64::zero() };
    Funds_uint64 used { Funds_uint64::zero() };
};

class Mempool {
    using iter_t = Txset::iterator;
    using const_iter_t = Txset::const_iter_t;

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
    Error insert_tx(const TransactionMessage& pm, TxHeight txh, const TxHash& hash, chainserver::DBCache& wartCache);
    void insert_tx_throw(const TransactionMessage& pm, TxHeight txh, const TxHash& hash, chainserver::DBCache& wartCache);
    void erase(TransactionId id);
    void set_free_balance(AccountToken, Funds_uint64 newBalance);
    void erase_from_height(Height);
    void erase_before_height(Height);

    // getters
    [[nodiscard]] auto cache_validity() const { return txs.cache_validity(); }
    [[nodiscard]] auto get_transactions(size_t n, NonzeroHeight height, std::vector<TxHash>* hashes = nullptr) const
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
    using BalanceEntries = std::map<AccountToken, LockedBalance>;
    using balance_iterator = BalanceEntries::iterator;
    void apply_logevent(const Put&);
    void apply_logevent(const Erase&);
    [[nodiscard]] std::pair<LockedBalance, std::optional<balance_iterator>> get_balance(AccountToken at, chainserver::DBCache&);
    [[nodiscard]] std::optional<TokenFunds> token_spend_throw(const TransactionMessage& pm, chainserver::DBCache& cache) const;
    void erase_internal(Txset::const_iter_t);
    bool erase_internal(Txset::const_iter_t, balance_iterator wartBalanceIter, bool gc = true);
    [[nodiscard]] balance_iterator create_or_get_balance_iter(AccountToken at, chainserver::DBCache& cache);
    void prune();

private:
    Updates updates;
    Txset txs;
    TokenData byToken;
    template <typename... Comparators>
    struct CombineIndices {
        static_assert(sizeof...(Comparators) > 0);
        using tuple_t = std::tuple<std::set<const_iter_t, Comparators>...>;
        template <size_t I>
        requires(I < sizeof...(Comparators))
        auto& get()
        {
            return std::get<I>(tuple);
        }
        template <size_t I>
        requires(I < sizeof...(Comparators))
        auto& get() const
        {
            return std::get<I>(tuple);
        }
        bool insert(const_iter_t iter)
        {
            struct check_t {
                size_t size;
                bool inserted;
            };
            std::optional<check_t> prev;
            std::apply([&](auto& set) {
                auto inserted { set.insert(iter).second };
                check_t next { set.size(), inserted };
                if (prev)
                    assert(*prev == next);
                else
                    prev = next;
            });
            return prev.value();
        }
        size_t erase(const_iter_t iter)
        {
            std::optional<size_t> prevErased;
            std::apply([&](auto& set) {
                auto erased { set.erase(iter) };
                if (prevErased)
                    assert(*prevErased == erased);
                else
                    prevErased = erased;
            });
            return prevErased.value();
        }
        auto size() const { return get<0>().size(); }

    private:
        tuple_t tuple;
    };
    struct : public CombineIndices<ComparatorPin, ComparatorTokenAccountFee, ComparatorAccountFee, ComparatorHash> {
        [[nodiscard]] const auto& pin() const { return get<0>(); }
        [[nodiscard]] const auto& account_token_fee() const { return get<1>(); }
        [[nodiscard]] const auto& account_fee() const { return get<2>(); }
        [[nodiscard]] const auto& hash() const { return get<3>(); }
    } index;
    ByFeeDesc byFee;
    BalanceEntries lockedBalances;
    bool master;
    size_t maxSize;
};
}
