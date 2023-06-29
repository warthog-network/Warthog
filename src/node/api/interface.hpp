#pragma once

#include "asyncio/conman.hpp"
#include "callbacks.hpp"
#include "eventloop/eventloop.hpp"
#include "global/globals.hpp"

// mempool cbunctions
void put_mempool(std::vector<uint8_t> data, ResultCb);
void get_mempool(MempoolCb cb);
void lookup_tx(const Hash hash, TxCb f);

// peer db functions
void get_banned_peers(PeerServer::BannedCB&& cb);
void unban_peers(ResultCb&& cb);

inline void get_offenses(Page page, PeerServer::OffensesCb&& cb)
{
    global().pps->async_get_offenses(page, std::move(cb));
}

// void get_connected_peers(Conman::PeersCB cb);

void get_failed_addresses(PeerServer::BannedCB cb);
void get_verified_addresses(PeerServer::BannedCB cb);

void get_connected_peers2(PeersCb&& cb);

// chain functions
void get_block_head(HeadCb cb);
void get_chain_mine(const Address& a, MiningCb cb);
void get_chain_header(Height height, HeaderCb cb);
void get_chain_hash(Height height, HashCb cb);
void get_chain_block(Height height, BlockCb cb);
void get_txcache(TxcacheCb&& cb);
void put_chain_append(MiningTask&& mt, ResultCb cb);
inline void get_signed_snapshot(Eventloop::SignedSnapshotCb&& cb)
{
    global().pel->defer(std::move(cb));
}

// sync functions
void get_headerdownload(HeaderdownloadCb f);

// account functions
void get_account_history(const Address& address, uint64_t end, HistoryCb cb);
void get_account_balance(const Address& address, BalanceCb cb);

// endpoints function
void inspect_eventloop(std::function<void(const Eventloop& e)>&&);
void inspect_conman(std::function<void(const Conman& c)>&&);
