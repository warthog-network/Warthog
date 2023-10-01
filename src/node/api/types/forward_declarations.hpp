#pragma once
#include <variant>
namespace API {
struct MempoolEntries;
struct TransferTransaction;
struct Head;
struct RewardTransaction;
struct Balance;
struct HashrateInfo;
struct Block;
struct History;
struct Richlist;
struct Peerinfo;
using Transaction = std::variant<RewardTransaction, TransferTransaction>;
}
