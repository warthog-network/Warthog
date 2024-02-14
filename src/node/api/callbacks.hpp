#pragma once
#include "api/types/forward_declarations.hpp"
#include "expected.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <variant>
#include <vector>

// forward declarations
class TransferTxExchangeMessage;
class Hash;
class TxHash;
class Grid;
class NonzeroHeight;
struct MiningTask;
namespace HeaderDownload {
class Downloader;
}
namespace chainserver {
struct TransactionIds;
}
struct NodeVersion {
    /* data */
};


class Header;

using PeersCb = std::function<void(const std::vector<API::Peerinfo>&)>;
using ResultCb = std::function<void(const tl::expected<void, int32_t>&)>;
using ConnectedConnectionCB = std::function<void(const API::PeerinfoConnections&)>;
using BalanceCb = std::function<void(const tl::expected<API::Balance, int32_t>&)>;

// using OffensesCb = std::function<void(const tl::expected<std, int32_t>&)>;
using MempoolCb = std::function<void(const tl::expected<API::MempoolEntries, int32_t>&)>;
using MempoolInsertCb = std::function<void(const tl::expected<TxHash, int32_t>&)>;
using MempoolTxsCb = std::function<void(std::vector<std::optional<TransferTxExchangeMessage>>&)>;
using MiningCb = std::function<void(const tl::expected<MiningTask, int32_t>&)>;
using TxcacheCb = std::function<void(const tl::expected<chainserver::TransactionIds, int32_t>&)>;
using HashrateCb = std::function<void(const tl::expected<API::HashrateInfo, int32_t>&)>;
using HashrateChartCb = std::function<void(const tl::expected<API::HashrateChart, int32_t>&)>;

using HeadCb = std::function<void(const tl::expected<API::Head, int32_t>&)>;
using RoundCb = std::function<void(const tl::expected<API::Round16Bit, int32_t>&)>;
using HeaderdownloadCb = std::function<void(const HeaderDownload::Downloader&)>;
using HeaderCb = std::function<void(const tl::expected<std::pair<NonzeroHeight,Header>, int32_t>&)>;
using HashCb = std::function<void(const tl::expected<Hash, int32_t>&)>;
using GridCb = std::function<void(const tl::expected<Grid, int32_t>&)>;
using TxCb = std::function<void(const tl::expected<API::Transaction, int32_t>&)>;
using LatestTxsCb = std::function<void(const tl::expected<API::TransactionsByBlocks, int32_t>&)>;
using BlockCb = std::function<void(const tl::expected<API::Block, int32_t>&)>;
using HistoryCb = std::function<void(const tl::expected<API::AccountHistory, int32_t>&)>;
using RichlistCb = std::function<void(const tl::expected<API::Richlist, int32_t>&)>;

using VersionCb = std::function<void(const tl::expected<NodeVersion, int32_t>&)>;
