#include "interface.hpp"
#include "api/types/all.hpp"
#include "asyncio/conman.hpp"
#include "chainserver/server.hpp"
#include "eventloop/eventloop.hpp"
#include "global/globals.hpp"

// mempool functions
void put_mempool(PaymentCreateMessage&& m, MempoolInsertCb cb)
{
    global().chainServer->api_put_mempool(std::move(m), std::move(cb));
}

void get_mempool(MempoolCb cb)
{
    global().chainServer->api_get_mempool(std::move(cb));
}

void lookup_tx(const Hash hash, TxCb f)
{
    global().chainServer->api_lookup_tx(hash, std::move(f));
}

void get_latest_transactions(LatestTxsCb f)
{
    global().chainServer->api_lookup_latest_txs(std::move(f));
};

// peer db functions

void get_banned_peers(PeerServer::banned_callback_t&& f)
{
    global().peerServer->async_get_banned(std::move(f));
}
void unban_peers(ResultCb&& f)
{
    global().peerServer->async_unban(std::move(f));
}
void get_offense_entries(ResultCb&& f)
{
    global().peerServer->async_unban(std::move(f));
}
// void get_connected_peers(Conman::PeersCB f)
// {
//     global().pcm->async_get_peers(f);
// }
void get_connected_peers2(PeersCb&& cb)
{
    global().core->api_get_peers(std::move(cb));
}

void get_connected_connection(ConnectedConnectionCB&& cb)
{
    global().core->api_get_peers([cb = std::move(cb)](const std::vector<API::Peerinfo>& pi) {
        cb({ pi });
    });
}

void get_round16bit_e8(uint64_t e8, RoundCb cb)
{
    cb(API::Round16Bit { Funds(e8) });
};

void get_round16bit_funds(Funds f, RoundCb cb)
{
    cb(API::Round16Bit { f });
};
void get_version(VersionCb cb)
{
    cb(NodeVersion {});
};

// chain functions
void get_block_head(HeadCb f)
{
    global().chainServer->async_get_head(f);
}
void get_chain_mine(const Address& a, MiningCb f)
{
    global().chainServer->api_get_mining(a, f);
}
mining_subscription::MiningSubscription subscribe_chain_mine(Address address, mining_subscription::callback_t callback)
{
    return global().chainServer->api_subscribe_mining(address,std::move(callback));
}

void get_chain_header(API::HeightOrHash hh, HeaderCb f)
{
    global().chainServer->api_get_header(hh, f);
}
void get_chain_hash(Height hh, HashCb f)
{
    global().chainServer->api_get_hash(hh, f);
}

void get_chain_grid(GridCb f)
{
    global().chainServer->api_get_grid(f);
}
void get_chain_block(API::HeightOrHash hh, BlockCb cb)
{
    global().chainServer->api_get_block(hh, cb);
}

void get_txcache(TxcacheCb&& cb)
{
    global().chainServer->api_get_txcache(std::move(cb));
}

void get_hashrate(HashrateCb&& cb)
{
    global().core->api_get_hashrate(std::move(cb));
}
void get_hashrate_chart(NonzeroHeight from, NonzeroHeight to, HashrateChartCb&& cb)
{
    global().core->api_get_hashrate_chart(from, to, std::move(cb));
}

void put_chain_append(MiningTask&& mt, ResultCb f)
{
    global().chainServer->api_mining_append(std::move(mt.block), f);
}
void get_signed_snapshot(Eventloop::SignedSnapshotCb&& cb)
{
    global().core->defer(std::move(cb));
}

// account functions
void get_account_balance(const API::AccountIdOrAddress& address, BalanceCb f)
{
    global().chainServer->api_get_balance(address, f);
}

void get_account_history(const Address& address, uint64_t beforeId,
    HistoryCb f)
{
    global().chainServer->api_get_history(address, beforeId, f);
}

void get_account_richlist(RichlistCb f)
{
    global().chainServer->api_get_richlist(f);
}

void inspect_conman(std::function<void(const UV_Helper& e)>&& cb)
{
    global().conman->async_inspect(std::move(cb));
}

void inspect_eventloop(std::function<void(const Eventloop& e)>&& cb)
{
    global().core->api_inspect(std::move(cb));
}
