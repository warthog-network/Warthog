#include "json.hpp"
#include "api/types/all.hpp"
#include "block/header/header_impl.hpp"
#include "block/header/view.hpp"
#include "chainserver/transaction_ids.hpp"
#include "communication/mining_task.hpp"
#include "communication/rxtx_server/api_types.hpp"
#include "crypto/crypto.hpp"
#include "eventloop/eventloop.hpp"
#include "eventloop/sync/header_download/header_download.hpp"
#include "general/hex.hpp"
#include "general/is_testnet.hpp"
#include "transport/helpers/tcp_sockaddr.hpp"
#include <nlohmann/json.hpp>
#include <ranges>

using namespace std::chrono;
using namespace nlohmann;
struct Inspector {
    static auto header_download(const Eventloop& e)
    {
        auto& d { e.headerDownload };
        json j;
        j["minWork"] = d.minWork.to_string();
        j["verifierMapSize"] = d.verifierMap.size();
        j["leaderListSize"] = d.leaderList.size();
        j["config"] = json {
            { "maxLeaders", d.maxLeaders },
            { "pendingDepth", d.pendingDepth }
        };
        {
            json qb;
            for (auto& [header, node] : d.queuedBatches) {
                qb[serialize_hex(header)]
                    = json {
                          { "batchSize", node.batch.size() },
                          { "originId", node.originId },
                          { "probeRefsSize", node.probeRefs.size() },
                          { "leaderRefsSize", node.leaderRefs.size() }
                      };
            }
            j["queuedBatches"] = qb;
        }
        return j.dump(1);
    }
    // static std::string endpoint_timers(const Eventloop& c)
    // {
    //     auto now = steady_clock::now();
    //     auto& m = c.connections;
    //     using VerIter = decltype(c.connections)::VerIter;
    //     using PinIter = decltype(c.connections)::PinIter;
    //     json j;
    //     json verifiers = json::array();
    //     json pins = json::array();
    //     for (auto& [t, v] : m.timer) {
    //         json e;
    //         e["expiresSeconds"] = duration_cast<seconds>(t - now).count();
    //         if (std::holds_alternative<VerIter>(v)) {
    //             auto& iter = std::get<VerIter>(v);
    //             auto& n = iter->second;
    //             e["endpoint"] = iter->first.to_string();
    //             e["seenSecondsAgo"] = n.outboundConnection ? 0 : duration_cast<seconds>(now - n.lastVerified).count();
    //             verifiers.push_back(e);
    //         } else {
    //             assert(std::holds_alternative<PinIter>(v));
    //             auto& iter = std::get<PinIter>(v);
    //             e["endpoint"] = iter->first.to_string();
    //             e["sleepOnFailedSeconds"] = iter->second.sleepSeconds;
    //             pins.push_back(e);
    //         }
    //     }
    //     j["verifiers"] = verifiers;
    //     j["pins"] = pins;
    //     return j.dump(1);
    // }
};

namespace jsonmsg {
namespace {
std::string format_utc(uint32_t timestamp)
{
    std::chrono::system_clock::time_point tp { std::chrono::seconds(timestamp) };
    auto tt { std::chrono::system_clock::to_time_t(tp) };
    auto utc_f = *std::gmtime(&tt);
    std::string out;
    out.resize(30);
    auto len { std::strftime(&out[0], out.size(), "%F %T UTC", &utc_f) };
    out.resize(len);
    return out;
}
void json_add_temporal(json& j, const api::TemporalInfo& tx)
{
    j["confirmations"] = tx.confirmations;
    j["blockHeight"] = tx.height.value();
    j["utc"] = format_utc(tx.timestamp);
    j["timestamp"] = tx.timestamp;
}

json to_json_history_base(const api::block::HistoryDataBase& hb)
{
    return {
        { "txHash", serialize_hex(hb.txhash) },
        { "historyId", (hb.hid ? json(hb.hid->value()) : json(nullptr)) },
    };
}
[[nodiscard]] json to_json_signed_info(const api::block::SignedInfoData& d, const char* originLabel)
{
    auto j(to_json_history_base(d));
    j[originLabel] = d.originAddress.to_string();
    j["fee"] = to_json(d.fee);
    j["nonceId"] = d.nonceId.value();
    j["pinHeight"] = d.pinHeight.value();
    return j;
}
template <typename T>
json verified_json(const std::map<TCPPeeraddr, T>& map)
{
    using namespace std::chrono;
    auto now = steady_clock::now();
    json e = json::array();
    for (auto& [a, n] : map) {
        json j;
        j["endpoint"] = a.to_string();
        j["seenSecondsAgo"] = n.outboundConnection ? 0 : duration_cast<seconds>(now - n.lastVerified).count();
        j["outboundConnection"] = n.outboundConnection;

        e.push_back(j);
    }
    return e;
}
json endpoint_json(auto& v)
{
    json e = json::array();
    for (const auto& ae : v) {
        e.push_back(ae.to_string());
    }
    return e;
}

json grid_json(const Grid& g)
{
    json out = json::array();
    for (auto header : g) {
        out.push_back(serialize_hex(header));
    }
    return out;
}

json header_json(const Header& header, NonzeroHeight height)
{
    auto version { header.version() };
    const bool testnet { is_testnet() };
    auto powVersion { POWVersion::from_params(height, version, testnet) };
    assert(powVersion.has_value());
    bool verusV2_2 { powVersion->uses_verus_2_2() };
    auto verusHash { verusV2_2 ? verus_hash_v2_2(header) : verus_hash_v2_1(header) };
    auto blockHash { header.hash() };
    auto sha256tHash { hashSHA256(blockHash) };
    auto target { header.target(height, testnet) };
    uint32_t targetBE = hton32(target.binary());
    json h;
    h["raw"] = serialize_hex(header.data(), header.size());
    h["timestamp"] = header.timestamp();
    h["utc"] = format_utc(header.timestamp());
    h["target"] = serialize_hex(targetBE);
    h["difficulty"] = target.difficulty();
    h["hash"] = serialize_hex(header.hash());
    h["pow"] = json {
        { "verusV2.2", verusV2_2 },
        { "hashVerus", serialize_hex(verusHash) },
        { "hashSha256t", serialize_hex(sha256tHash) },
        { "floatVerus", CustomFloat(verusHash).to_double() },
        { "floatSha256t", CustomFloat(sha256tHash).to_double() },
    };
    h["merkleroot"] = serialize_hex(header.merkleroot());
    h["nonce"] = serialize_hex(header.nonce());
    h["prevHash"] = serialize_hex(header.prevhash());
    h["version"] = serialize_hex(version.value());
    return h;
}

[[nodiscard]] json body_json(const api::Block& b)
{
    json j;
    auto& actions { b.actions };
    { // rewards
        json a = json::array();
        if (b.actions.reward) {
            a.push_back(tx_to_json(*b.actions.reward));
        }
        j["rewards"] = a;
    }

    auto gen_arr { [&](auto& arr) {
        json a(json::array());
        for (auto& e : arr)
            a.push_back(tx_to_json(e));
        return a;
    } };
    j["wartTransfers"] = gen_arr(actions.wartTransfers); // WART transfers
    j["tokenTransfers"] = gen_arr(actions.tokenTransfers);
    j["newOrders"] = gen_arr(actions.newOrders);
    j["matches"] = gen_arr(actions.matches);
    j["liquidityDeposits"] = gen_arr(actions.liquidityDeposit);
    j["LiquidityWithdrawals"] = gen_arr(actions.liquidityWithdrawal);
    j["cancelations"] = gen_arr(actions.cancelations);
    return j;
}

json amount_json(Funds_uint64 amt, AssetPrecision prec)
{
    return to_json(FundsDecimal(amt, prec));
}

json limit_json(Price_uint64 limit, AssetPrecision prec)
{
    return {
        { "exponent10", limit.base10_exponent(prec) },
        { "exponent2", limit.mantissa_exponent2() },
        { "mantissa", limit.mantissa_16bit() },
        { "double", limit.to_double_adjusted(prec) },
    };
}
} // namespace

using namespace nlohmann;

json to_json(Wart w)
{
    return {
        { "str", w.to_string() },
        { "E8", w.E8() }
    };
}
json to_json(const FundsDecimal& fd)
{
    return {
        { "str", fd.to_string() },
        { "u64", fd.funds.value() },
        { "precision", fd.precision.value() }
    };
}
namespace {
json transaction_type(const char* label, json& j)
{
    return { { "transaction", j }, { "type", label } };
}

}

json tx_to_json(const api::block::WartTransfer& tx)
{
    json j(to_json_signed_info(tx, "fromAddress"));
    j["toAddress"] = tx.toAddress.to_string();
    j["amount"] = to_json(tx.amount);
    return j;
}
json tx_to_json(const api::block::Reward& tx)
{
    auto j(to_json_history_base(tx));
    j["toAddress"] = tx.toAddress.to_string();
    j["amount"] = to_json(tx.wart);
    return j;
}

json tx_to_json(const api::block::TokenTransfer& tx)
{
    json j(to_json_signed_info(tx, "fromAddress"));
    j["toAddress"] = tx.toAddress.to_string();
    j["amount"] = to_json(tx.amount);
    j["token"] = to_json(tx.assetInfo);
    return j;
}

json tx_to_json(const api::block::AssetCreation& tx)
{
    json j(to_json_signed_info(tx, "creatorAddress"));
    j["supply"] = to_json(tx.supply.to_string());
    j["name"] = tx.name.to_string();
    j["assetId"] = (tx.assetId ? json(tx.assetId->value()) : json(nullptr));
    return j;
}

json tx_to_json(const api::block::NewOrder& tx)
{
    json j(to_json_signed_info(tx, "address"));
    j["asset"] = jsonmsg::to_json(tx.assetInfo); // TODO: check that asset exists when NewOrder goes to mempool
    j["amount"] = amount_json(tx.amount, tx.assetInfo.precision);
    j["limit"] = limit_json(tx.limit, tx.assetInfo.precision);
    j["buy"] = tx.buy;
    return j;
}

json tx_to_json(const api::block::Match& tx)
{
    auto j(to_json_history_base(tx));
    j["asset"] = to_json(tx.assetInfo);

    auto bq_json {
        [&](const auto& bq) -> json {
            return {
                { "base", to_json(bq.base().to_decimal(tx.assetInfo.precision)) },
                { "quote", to_json(bq.quote()) }
            };
        }
    };
    j["poolBefore"] = bq_json(tx.poolBefore);
    j["poolAfter"] = bq_json(tx.poolAfter);
    auto match_json {
        [&]<typename T>(const std::vector<T>& v) {
            json res = json::array();
            for (auto& s : v) {
                auto e { bq_json(s) };
                e["historyId"] = s.referred_history_id().value();
                res.push_back(std::move(e));
            }
            return res;
        }
    };
    j["buySwaps"] = match_json(tx.buySwaps);
    j["sellSwaps"] = match_json(tx.sellSwaps);
    return j;
}

json tx_to_json(const api::block::LiquidityDeposit& tx)
{
    json j(to_json_signed_info(tx, "address"));
    j["asset"] = jsonmsg::to_json(tx.assetInfo);
    j["baseDeposited"] = to_json(tx.baseDeposited.to_decimal(tx.assetInfo.precision));
    j["quoteDeposited"] = to_json(tx.quoteDeposited);
    j["sharesReceived"] = (tx.sharesReceived ? to_json(tx.sharesReceived->to_decimal(AssetPrecision::digits8())) : json(nullptr));
    return {};
}

json tx_to_json(const api::block::LiquidityWithdrawal& tx)
{
    json j(to_json_signed_info(tx, "address"));
    j["asset"] = jsonmsg::to_json(tx.assetInfo);
    j["sharesRedeemed"] = to_json(tx.sharesRedeemed.to_decimal(AssetPrecision::digits8()));
    j["baseReceived"] = (tx.baseReceived ? to_json(tx.baseReceived->to_decimal(tx.assetInfo.precision)) : json(nullptr));
    j["quoteReceived"] = (tx.quoteReceived ? to_json(*tx.quoteReceived) : json(nullptr));
    return {};
}

json tx_to_json(const api::block::Cancelation& tx)
{
    json j(to_json_signed_info(tx, "address"));
    return j;
}

json to_json(const PeerDB::BanEntry& item)
{
    return {
        { "ip", item.ip.to_string().c_str() },
        { "expires", item.banuntil },
        { "reasion", item.offense.err_name() },
    };
}

namespace {
json to_json(const api::ThrottleState::BatchThrottler& bt)
{
    return {
        { "h1", bt.h0.value() },
        { "h2", bt.h1.value() },
        { "window", bt.window }
    };
}
}

json to_json(const api::ThrottleState& ts)
{
    using namespace std::chrono;

    return {
        { "delay", duration_cast<seconds>(ts.delay).count() },
        { "blockRequest", to_json(ts.blockreq) },
        { "headerRequest", to_json(ts.batchreq) }
    };
}

json to_json(const Hash& h)
{
    json j;
    j["hash"] = serialize_hex(h);
    return j;
}

json to_json(const TxHash& h)
{
    json j;
    j["txHash"] = serialize_hex(h);
    return j;
}

json to_json(const api::ChainHead& ch)
{
    return {
        { "hash", serialize_hex(ch.hash) },
        { "height", ch.height },
        { "difficulty", ch.nextTarget.difficulty() },
        { "is_janushash", ch.nextTarget.is_janushash() },
        { "pinHeight", ch.pinHeight },
        { "worksum", ch.worksum.getdouble() },
        { "worksumHex", ch.worksum.to_string() },
        { "pinHash", serialize_hex(ch.pinHash) },
        { "hashrate", ch.hashrate },
    };
}

json to_json(const api::Head& h)
{
    auto j(to_json(h.chainHead));
    j["synced"] = h.synced;
    return j;
}

json to_json(const std::pair<NonzeroHeight, Header>& h)
{
    return json {
        { "header", header_json(h.second, h.first) }
    };
}

json to_json(const api::TransmissionTimeseries& tt)
{
    json arr;
    for (auto& [host, ts] : tt.byHost) {
        json tsjson(json::array());
        for (auto& e : ts) {
            tsjson.push_back({
                { "begin", e.begin.value() },
                { "end", e.end.value() },
                { "rx", e.rx },
                { "tx", e.tx },
            });
        }
        arr[host] = tsjson;
    }
    return arr;
}

json to_json(const api::MiningState& ms)
{
    auto& mt { ms.miningTask };
    json j;
    auto height { mt.block.height };
    auto blockReward { mt.block.body.reward };
    j["synced"] = ms.synced;
    j["header"] = serialize_hex(mt.block.header);
    j["difficulty"] = mt.block.header.target(height, is_testnet()).difficulty();
    j["merklePrefix"] = serialize_hex(mt.block.body.merkleLeaves.merkle_prefix());
    j["body"] = serialize_hex(mt.block.body.data);
    j["blockReward"] = to_json(blockReward.wart());
    j["height"] = height;
    j["testnet"] = is_testnet();
    return j;
}
json to_json(const api::MempoolEntries& entries)
{
    json j;
    json a = json::array();
    for (auto& e : entries.entries) {
        json elem;
        elem["fromAddress"] = e.from_address(e.txHash).to_string();
        elem["pinHeight"] = e.pin_height();
        elem["txHash"] = serialize_hex(e.txHash);
        elem["nonceId"] = e.nonce_id();
        elem["fee"] = to_json(e.fee());
        elem["toAddress"] = e.to_addr().to_string();
        elem["amount"] = to_json(e.wart());
        a.push_back(elem);
    }
    j["data"] = a;
    return j;
}

json to_json(const api::Transaction& tx)
{
    return std::visit([&](const auto& e) {
        json j(tx_to_json(e));
        json_add_temporal(j, e);
        return transaction_type(e.label, j);
    },
        tx);
}

json to_json(const Peeraddr& ea)
{
    return ea.to_string();
}

json to_json(const api::PeerinfoConnections& pc)
{
    return to_json(pc.v, pc.map);
}

json to_json(const api::TransactionsByBlocks& txs)
{
    json arr = json::array();
    for (auto iter = txs.blocks_reversed.begin(); iter != txs.blocks_reversed.end(); ++iter) {
        auto& block { *iter };
        arr.push_back(
            json {
                { "header", header_json(block.header, block.height) },
                { "body", body_json(block) },
                { "timestamp", block.header.timestamp() },
                { "utc", format_utc(block.header.timestamp()) },
                { "confirmations", block.confirmations },
                { "height", block.height } });
    }
    return json {
        { "fromId", txs.fromId },
        { "count", txs.count },
        { "perBlock", arr }
    };

    return arr;
}

json to_json(const api::Block& block)
{
    json j;
    HeaderView hv(block.header.data());
    j["header"] = header_json(block.header, block.height);
    j["body"] = body_json(block);
    j["timestamp"] = hv.timestamp();
    j["utc"] = format_utc(hv.timestamp());
    j["confirmations"] = block.confirmations;
    j["height"] = block.height;
    return j;
}
json to_json(const api::BlockSummary& block)
{
    json j;
    HeaderView hv(block.header.data());
    j["header"] = header_json(block.header, block.height);
    j["confirmations"] = block.confirmations;
    j["height"] = block.height;
    j["nTransfers"] = block.nTransfers;
    j["miner"] = block.miner.to_string();
    j["transferred"] = to_json(block.transferred);
    j["totalTxFee"] = to_json(block.totalTxFee);
    auto r { block.height.reward() };
    j["blockReward"] = to_json(r);
    return j;
}

json to_json(const api::AccountHistory& h)
{
    json a = json::array();
    auto& reversed = h.blocks_reversed;
    for (size_t i = 0; i < reversed.size(); ++i) {
        auto& b = reversed[reversed.size() - 1 - i];
        json elem;
        elem["height"] = b.height;
        elem["confirmations"] = b.confirmations;
        elem["transactions"] = body_json(b);
        a.push_back(elem);
    }
    json j;
    j["perBlock"] = a;
    j["fromId"] = h.fromId;
    j["balance"] = to_json(h.balance);
    return j;
}

json to_json(const api::AddressCount& ac)
{
    return {
        { "address", ac.address.to_string() },
        { "count", ac.count }
    };
}

json to_json(const api::Richlist& l)
{
    json a = json::array();
    for (auto& [address, balance] : l.entries) {
        json elem;
        elem["address"] = address.to_string();
        elem["balance"] = to_json(balance);
        a.push_back(elem);
    }
    return a;
}

nlohmann::json to_json(const api::Wallet& w)
{
    auto pubKey { w.pk.pubkey() };
    return {
        { "privKey", w.pk.to_string() },
        { "pubKey", pubKey.to_string() },
        { "address", pubKey.address().to_string() }
    };
}

json to_json(const api::HashrateInfo& hi)
{
    return json {
        { "lastNBlocksEstimate", hi.estimate },
        { "N", hi.nBlocks }
    };
}

json to_json(const api::HashrateBlockChart& c)
{
    json data(json::array());
    for (const auto& v : c.chart) {
        data.push_back(v);
    }
    return json {
        { "range", json { { "min", c.range.begin }, { "max", c.range.end } } },
        { "data", data }
    };
}
json to_json(const api::HashrateTimeChart& c)
{
    json data(json::array());
    for (const auto& [timestamp, height, hashrate] : std::ranges::reverse_view(c.chartReversed)) {
        data.push_back({ { "timestamp", timestamp },
            { "height", height.value() },
            { "hashrate", hashrate } });
    }
    return json {
        { "chart", data },
        { "begin", c.begin },
        { "end", c.end },
        { "interval", c.interval },
    };
}

json to_json(const OffenseEntry& e)
{
    return json {
        { "ip", e.ip.to_string().c_str() },
        { "timestamp", e.timestamp },
        { "utc", format_utc(e.timestamp) },
        { "offense", e.offense.err_name() }
    };
}

json to_json(const api::ThrottledPeer& pi)
{
    return {
        { "throttle", to_json(pi.throttle) },
        { "connection",
            json {
                { "endpoint", pi.endpoint.to_string() },
                { "id", pi.id },
            } },
    };
}
json to_json(const api::Peerinfo& pi)
{
    json elem;
    auto conn = json {
        { "port", pi.endpoint.port() },
        { "sinceTimestamp", pi.since },
        { "sinceUTC", format_utc(pi.since) }
    };
    if (auto ip { pi.endpoint.ip() }; ip.has_value())
        conn["ip"] = ip->to_string();
    else
        conn["ip"] = nullptr;

    elem["throttle"] = to_json(pi.throttle);
    elem["connection"] = conn;

    elem["leaderPriority"] = json {
        { "ack", json { { "importance", pi.acknowledgedSnapshotPriority.importance }, { "height", pi.acknowledgedSnapshotPriority.height } } },
        { "theirs", json { { "importance", pi.theirSnapshotPriority.importance }, { "height", pi.theirSnapshotPriority.height } } }
    };
    elem["chain"] = json {
        { "length", pi.chainstate.descripted()->chain_length() },
        { "forkLower", pi.chainstate.consensus_fork_range().lower() },
        { "forkUpper", pi.chainstate.consensus_fork_range().upper() },
        { "descriptor", pi.chainstate.descripted()->descriptor },
        { "worksum", pi.chainstate.descripted()->worksum().getdouble() },
        { "worksumHex", pi.chainstate.descripted()->worksum().to_string() },
        { "grid", grid_json(pi.chainstate.descripted()->grid()) }
    };
    return elem;
}

json to_json(const TCPPeeraddr& a)
{
    return a.to_string();
}

json to_json(const api::WartBalance& b)
{
    json j;
    j["balance"] = to_json(b.balance);
    if (b.address) {
        j["address"] = b.address->address.to_string();
        j["accountId"] = b.address->accountId.value();
    } else {
        j["address"] = nullptr;
        j["accountId"] = nullptr;
    }
    return j;
}

json to_json(const Grid& g)
{
    json j(json::array());
    for (const auto& h : g) {
        j.push_back(serialize_hex(h));
    }
    return j;
}

json to_json(const std::optional<SignedSnapshot>& sp)
{
    if (sp) {
        auto& s = *sp;
        return json {
            { "priority", json { { "height", s.priority.height }, { "importance", s.priority.importance } } },
            { "hash", serialize_hex(s.hash) },
            { "signature", s.signature.to_string() },
        };
    }
    return nullptr;
}

json to_json(const TransactionId& txid)
{
    return json {
        { "accountId", txid.accountId },
        { "nonceId", txid.nonceId },
        { "pinHeight", txid.pinHeight },
    };
}

json to_json(const AssetBasic& t)
{
    return {
        { "hash", serialize_hex(t.hash) },
        { "id", t.id.value() },
        { "name", t.name.to_string() },
        { "precision", t.precision.value() }
    };
}

json to_json(const chainserver::TransactionIds& txids)
{
    json j(json::array());
    for (auto& txid : txids) {
        j.push_back(json {
            { "accountId", txid.accountId },
            { "nonceId", txid.nonceId },
            { "pinHeight", txid.pinHeight },
        });
    }
    return j;
}

nlohmann::json to_json(const api::Round16Bit& e)
{
    auto c { CompactUInt::compact(e.original) };
    return json {
        { "original", to_json(e.original) },
        { "rounded", to_json(c.uncompact()) },
        { "16bit", c.value() }
    };
}

nlohmann::json to_json(const PrintNodeVersion&)
{
    return json {
        { "name", CMDLINE_PARSER_VERSION },
        { "major", VERSION_MAJOR },
        { "minor", VERSION_MINOR },
        { "patch", VERSION_PATCH },
        { "commit", GIT_COMMIT_INFO }
    };
}

nlohmann::json to_json(const api::Rollback& rb)
{
    return json {
        { "length", rb.length }
    };
}

nlohmann::json to_json(const api::IPCounter& ipc)
{
    json obj(json::object());
    for (auto& [ip, c] : ipc.vector) {
        obj.push_back({ ip.to_string(), c });
    }
    return obj;
}

nlohmann::json to_json(const api::NodeInfo& info)
{
    using namespace std;
    using namespace std::chrono;
    using namespace std::string_literals;
    using sc = std::chrono::steady_clock;
    auto format_duration = [](size_t s /* uptime seconds*/) {
        size_t days = s / (24 * 60 * 60);
        size_t hours = (s % (24 * 60 * 60)) / (60 * 60);
        size_t minutes = (s % (60 * 60)) / 60;
        size_t seconds = (s % 60);
        return to_string(days) + "d "s + to_string(hours) + "h "s + to_string(minutes) + "m "s + to_string(seconds) + "s"s;
    };

    auto startedAt { config().started_at() };
    auto uptime { sc::now() - startedAt.steady };
    auto uptimeSeconds(duration_cast<seconds>(uptime).count());
    auto uptimeStr { format_duration(uptimeSeconds) };
    uint32_t sinceTimestamp(duration_cast<seconds>(startedAt.system.time_since_epoch()).count());
    return {
        { "dbSize", info.dbSize },
        { "chainDBPath", config().data.chaindb },
        { "peersDBPath", config().data.peersdb },
        { "rxtxDBPath", config().data.rxtxdb },
        { "version", { { "name", CMDLINE_PARSER_VERSION }, { "major", VERSION_MAJOR }, { "minor", VERSION_MINOR }, { "patch", VERSION_PATCH }, { "commit", GIT_COMMIT_INFO } } },
        { "uptime", { { "sinceTimestamp", sinceTimestamp }, { "sinceUTC", format_utc(sinceTimestamp) }, { "seconds", uptimeSeconds }, { "formatted", uptimeStr }

                    } }
    };
}

std::string serialize(const api::Raw& r)
{
    return r.s;
}

// std::string endpoints(const Eventloop& e)
// {
//     auto [verified, failed, unverified, pending] = Inspector::endoints(e);
//     json j;
//     j["verified"] = verified_json(*verified);
//     j["failed"] = endpoint_json(*failed);
//     j["unverified"] = endpoint_json(*unverified);
//     auto a = *pending;
//     j["pending"] = pending_json(*pending);
//     return j.dump(1);
// }

// std::string connect_timers(const Eventloop& e)
// {
//     return Inspector::endpoint_timers(e);
// }
std::string header_download(const Eventloop& e)
{
    return Inspector::header_download(e);
}

// SubscriptionAction parse_subscribe_throw(std::string_view s)
// {
//     auto j { json::parse(s) };
//     if (j.size() != 1 || !j.is_object())
//         goto failed;
//
//     if (auto iter { j.find("subscribe") }; iter != j.end()) {
//         return { true, iter.value().get<std::string>() };
//     }
//     if (auto iter { j.find("unsubscribe") }; iter != j.end()) {
//         return { false, iter.value().get<std::string>() };
//     }
// failed:
//     throw std::runtime_error("Cannot parse subscription " + std::string(s));
// };

} // namespace jsonmsg
