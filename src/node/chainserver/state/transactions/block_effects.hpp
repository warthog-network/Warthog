#pragma once
#include "block/body/rollback.hpp"
#include "chainserver/db/chain_db.hpp"
#include "chainserver/db/types.hpp"
#include "defi/token/account_token.hpp"
#include "general/funds.hpp"
#include <vector>

namespace block_apply {

struct BalanceUpdate {
    AccountToken at;
    BalanceId id;
    Funds_uint64 original;
    Funds_uint64 updated;
};
using OrderDelete = chain_db::OrderData;
struct PoolUpdate {
    PoolUpdate(const PoolData& pd)
        : original(pd.asset_id(), pd)
        , updated(pd)
    {
    }
    bool nonzero() const
    {
        return !updated.base.is_zero() && !updated.quote.is_zero() && (updated.shares_total() != 0);
    }
    rollback::Poolstate original;
    defi::Pool_uint64 updated;
};
struct OrderUpdate {
    chain_db::OrderFillstate newFillState;
    Funds_uint64 originalFilled;
    chain_db::OrderFillstate original_fill_state() const
    {
        return {
            .id { newFillState.id },
            .filled { originalFilled },
            .buy = newFillState.buy
        };
    }
};
class RollbackTrackingDB {
public:
    RollbackTrackingDB(ChainDB& db)
        : db(db)
        , rg(db)
    {
    }
    void insert_account(const AddressView address, AccountId verifyNextId)
    {
        db.insert_account(address, verifyNextId);
    }
    void update_balance(const BalanceUpdate& u)
    {
        db.set_balance(u.id, u.updated);
        rg.register_original_balance({ u.id, u.original });
    }
    void insert_token_balance(AccountToken at, Funds_uint64 balance)
    {
        db.insert_token_balance(at, balance);
    }
    void delete_order(OrderDelete od)
    {
        db.delete_order({ .id = od.id, .buy = od.buy });
        rg.register_original_order(std::move(od));
    }
    void insert_order(const chain_db::OrderData& o)
    {
        db.insert_order(o);
    }
    void update_order_fillstate(const OrderUpdate& o)
    {
        db.update_order_fillstate(o.newFillState);
        rg.register_original_fillstate(o.original_fill_state());
    }
    void insert_pool(const PoolData& p)
    {
        db.insert_pool(p);
    }
    void update_pool(const PoolUpdate& u)
    {
        rg.register_original_poolstate(u.original);
        db.update_pool_liquidity(u.original.id, u.updated);
    }
    auto rollback_data() &&
    {
        return std::move(rg);
    }

private:
    ChainDB& db;
    rollback::Data rg;
};

struct BlockEffects {
    std::vector<BalanceUpdate> updateBalances;
    std::vector<std::tuple<AccountToken, Funds_uint64>> insertBalances;
    std::vector<std::tuple<AddressView, AccountId>> insertAccounts;
    std::vector<OrderDelete> deleteCanceledOrders;
    std::vector<chain_db::AssetData> insertAssetCreations;
    std::vector<TransactionId> insertCancelOrder;
    std::vector<chain_db::OrderData> OrderInsertions;
    std::vector<OrderUpdate> orderUpdates;
    std::vector<OrderDelete> orderDeletes;
    std::vector<PoolUpdate> poolUpdates;
    std::vector<PoolData> poolInsertions;
    std::set<TransactionId> canceledTxids;
    std::set<HistoryId> canceledOrderIds;
    [[nodiscard]] auto apply(RollbackTrackingDB db)
    {
        // Checklist for different transaction types
        // insertAccounts:       generate [ ], apply [x], rollback [ ]
        // updateBalances:       generate [ ], apply [x], rollback [ ]
        // insertBalances:       generate [ ], apply [x], rollback [ ]
        // deleteOrders:         generate [ ], apply [x], rollback [ ]
        // insertAssetCreations: generate [ ], apply [ ], rollback [ ]
        // insertOrders:         generate [ ], apply [ ], rollback [ ]
        // insertCancelOrder:    generate [ ], apply [ ], rollback [ ]
        // matchDeltas:          generate [ ], apply [ ], rollback [ ]

        // new accounts
        for (auto& [address, accId] : insertAccounts)
            db.insert_account(address, accId);
        // update old balances
        for (auto& u : updateBalances)
            db.update_balance(u);
        // insert new balances
        for (auto& [at, bal] : insertBalances)
            db.insert_token_balance(at, bal);

        for (auto& d : orderDeletes)
            db.delete_order(d);
        for (auto& o : OrderInsertions) // new orders
            db.insert_order(o);
        for (auto& o : orderUpdates)
            db.update_order_fillstate(o);
        for (auto& p : poolInsertions)
            db.insert_pool(p);
        for (auto& p : poolUpdates)
            db.update_pool(p);

        // insert asset creations
        // for (auto& tc : prepared.insertAssetCreations)
        //     db.insert_new_asset(tc);
        return std::move(db).rollback_data();
    }
};
}
