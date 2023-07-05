#include "interface.hpp"
#include "asyncio/conman.hpp"
#include "chainserver/server.hpp"
#include "eventloop/eventloop.hpp"
#include "global/globals.hpp"

// mempool functions
void put_mempool(std::vector<uint8_t> data, ResultCb cb)
{
    global().pcs->api_put_mempool(std::move(data), std::move(cb));
};

void get_mempool(MempoolCb cb)
{
    global().pcs->api_get_mempool(std::move(cb));
};

void lookup_tx(const Hash hash, TxCb f)
{
    global().pcs->api_lookup_tx(hash, std::move(f));
};

// peer db functions

void get_banned_peers(PeerServer::BannedCB&& f)
{
    global().pps->async_get_banned(std::move(f));
};
void unban_peers(ResultCb&& f)
{
    global().pps->async_unban(std::move(f));
};
void get_offense_entries(ResultCb&& f)
{
    global().pps->async_unban(std::move(f));
};
// void get_connected_peers(Conman::PeersCB f)
// {
//     global().pcm->async_get_peers(f);
// };
void get_connected_peers2(PeersCb&& cb)
{
    global().pel->api_get_peers(std::move(cb));
};

// chain functions
void get_block_head(HeadCb f)
{
    global().pcs->async_get_head(f);
};
void get_chain_mine(const Address& a, MiningCb f)
{
    global().pcs->api_get_mining(a, f);
};

void get_chain_header(Height height, HeaderCb f)
{
    global().pcs->api_get_header(height, f);
};
void get_chain_hash(Height height, HashCb f)
{
    global().pcs->api_get_hash(height, f);
};

void get_chain_grid(GridCb f){
    global().pcs->api_get_grid(f);
};
void get_chain_block(Height height, BlockCb cb)
{
    global().pcs->api_get_block(height, cb);
};

void get_txcache(TxcacheCb&& cb)
{
    global().pcs->api_get_txcache(std::move(cb));
};

void put_chain_append(MiningTask&& mt, ResultCb f)
{
    global().pcs->api_mining_append(std::move(mt.block), f);
};

// account functions
void get_account_balance(const Address& address, BalanceCb f)
{
    global().pcs->api_get_balance(address, f);
};

void get_account_history(const Address& address, uint64_t beforeId,
    HistoryCb f)
{
    global().pcs->api_get_history(address, beforeId, f);
};

void inspect_conman(std::function<void(const Conman& e)>&& cb)
{
    global().pcm->async_inspect(std::move(cb));
};

void inspect_eventloop(std::function<void(const Eventloop& e)>&& cb)
{
    global().pel->api_inspect(std::move(cb));
};
