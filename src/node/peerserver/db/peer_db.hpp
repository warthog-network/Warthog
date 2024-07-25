#pragma once
#include "SQLiteCpp/SQLiteCpp.h"
#include "general/errors.hpp"
#include "general/now.hpp"
#include "general/page.hpp"
#include "general/timestamp.hpp"
#include "offense_entry.hpp"
#include "transport/helpers/tcp_sockaddr.hpp"
#include <vector>

inline IP::BanHandle get_banhandle(SQLite::Column col0)
{
    if (col0.isInteger())
        return IPv4(uint32_t(col0.getInt64()));

    assert(col0.isBlob());
    size_t n(col0.size());
    if (n==32) {
        return IPv6::BanHandle32((const uint8_t*)col0.getBlob(),n);
    }else if (n == 48){
        return IPv6::BanHandle48((const uint8_t*)col0.getBlob(),n);
    }
    throw std::runtime_error("Peers database error in, cannot get ban handle");
}

inline IP get_ip(SQLite::Column col0)
{
    if (col0.isInteger())
        return IPv4(uint32_t(col0.getInt64()));

    assert(col0.isBlob());
    size_t n(col0.size());
    assert(n == IPv6::byte_size());
    return IPv6::from_data((uint8_t*)col0.getBlob(), IPv6::byte_size());
}
struct Peeraddr;
class PeerDB {
private:
    // ids to save additional information in tables
    static constexpr int64_t WORKSUMID = -1;

public:
    struct BanEntry {
        IP::BanHandle ip;
        int32_t banuntil;
        Error offense;
        BanEntry(IP::BanHandle ip, int32_t banuntil, uint32_t offense)
            : ip(ip)
            , banuntil(banuntil)
            , offense(offense) {};
    };
    PeerDB(const std::string& path);
    SQLite::Transaction transaction() { return SQLite::Transaction(db); }
    void set_ban(IPv4 ipv4, uint32_t banUntil, int32_t offense)
    {
        peerban.bind(1, banUntil);
        peerban.bind(2, offense);
        peerban.bind(3, ipv4.data);
        peerban.exec();
        peerban.reset();
    }
    void set_ban(IPv6::Block48View block48, uint32_t banUntil, int32_t offense)
    {
        peerban.bind(1, banUntil);
        peerban.bind(2, offense);
        peerban.bindNoCopy(3, block48.data(), block48.size());
        peerban.exec();
        peerban.reset();
    }
    void set_ban(IPv6::Block32View block32, uint32_t banUntil, int32_t offense)
    {
        peerban.bind(1, banUntil);
        peerban.bind(2, offense);
        peerban.bindNoCopy(3, block32.data(), block32.size());
        peerban.exec();
        peerban.reset();
    }
    void insert_clear_ban(IP ip)
    {
        if (ip.is_v4()) {
            insertClearBan.bind(1, ip.get_v4().data);
        } else {
            auto block48 { ip.get_v6().block48_view() };
            peerban.bindNoCopy(1, block48.data(), block48.size());
        }
        insertClearBan.exec();
        insertClearBan.reset();
    }

    std::vector<BanEntry> get_banned_peers()
    {
        std::vector<BanEntry> res;
        uint32_t t = now_timestamp();
        selectCurrentBans.bind(1, t);
        while (selectCurrentBans.executeStep()) {
            auto banhandle { get_banhandle(selectCurrentBans.getColumn(0)) };
            uint32_t banuntil = selectCurrentBans.getColumn(1).getInt64();
            int32_t offense = selectCurrentBans.getColumn(2).getInt64();
            BanEntry e(banhandle, banuntil, offense);
            res.push_back(e);
        }
        selectCurrentBans.reset();
        return res;
    }

    struct GetPeerResult {
        Timestamp banUntil;
        int32_t offense;
    };
    // std::optional<GetPeerResult> get_peer(IPv4 ipv4, uint32_t& banUntil, int32_t& offense)
    [[nodiscard]] std::optional<GetPeerResult> get_peer(const IP& ip)
    {
        std::optional<GetPeerResult> res;
        if (ip.is_v4()) {
            selectBan.bind(1, ip.get_v4().data);
        } else {
            auto v { ip.get_v6().block48_view() };
            selectBan.bindNoCopy(1, v.data(), v.size());
        }
        if (selectBan.executeStep()) {
            res.emplace(GetPeerResult {
                .banUntil = Timestamp(selectBan.getColumn(0).getInt64()),
                .offense = selectBan.getColumn(1).getInt() });
        }
        selectBan.reset();
        return res;
    }

    int64_t insert_connect(IP ip, bool inbound, uint32_t begin)
    {
        if (ip.is_v4()) {
            connection_log_insert.bind(1, ip.get_v4().data);
        } else {
            auto& v6 { ip.get_v6() };
            connection_log_insert.bindNoCopy(1, v6.data.data(), v6.data.size());
        }
        connection_log_insert.bind(2, inbound ? 1 : 0);
        connection_log_insert.bind(3, begin);
        connection_log_insert.exec();
        connection_log_insert.reset();
        return db.getLastInsertRowid();
    }

    void insert_disconnect(int64_t rowid, uint32_t end, int32_t code)
    {
        connection_log_update.bind(1, end);
        connection_log_update.bind(2, code);
        connection_log_update.bind(3, rowid);
        connection_log_update.exec();
        connection_log_update.reset();
    }

    void insert_refuse(IP ip, uint32_t timestamp)
    {
        if (ip.is_v4()) {
            refuseinsert.bind(1, ip.get_v4().data);
        } else {
            auto& v6 { ip.get_v6() };
            refuseinsert.bindNoCopy(1, v6.data.data(), v6.data.size());
        }
        refuseinsert.bind(2, timestamp);
        refuseinsert.exec();
        refuseinsert.reset();
    }
    void insert_offense(IPv4 ipv4, int32_t offense)
    {
        assert(offense != 0);
        insertOffense.bind(1, ipv4.data);
        insertOffense.bind(2, now_timestamp());
        insertOffense.bind(3, offense);
        insertOffense.exec();
        insertOffense.reset();
    }
    void insert_offense(IPv6 ipv6, int32_t offense)
    {
        assert(offense != 0);
        insertOffense.bind(1, (void*)ipv6.data.data(), ipv6.data.size());
        insertOffense.bind(2, now_timestamp());
        insertOffense.bind(3, offense);
        insertOffense.exec();
        insertOffense.reset();
    }
    std::vector<OffenseEntry> get_offenses(Page page)
    {
        assert(page.val() > 0);
        std::vector<OffenseEntry> out;
        getOffenses.bind(1, (page.val() - 1) * 100);
        while (getOffenses.executeStep()) {
            auto ip { get_ip(getOffenses.getColumn(0)) };
            uint32_t timestamp
                = getOffenses.getColumn(1).getInt64();
            int32_t offense = getOffenses.getColumn(2).getInt64();
            out.push_back({ ip, timestamp, offense });
        }
        getOffenses.reset();
        return out;
    }
    void reset_bans()
    {
        stmtResetBans.exec();
        stmtResetBans.reset();
    }
    std::vector<std::pair<TCPPeeraddr, Timestamp>> recent_peers(int64_t maxEntries = 100);
    void peer_seen(TCPPeeraddr, uint32_t now);
    void peer_insert(TCPPeeraddr);

private:
    SQLite::Database db;
    struct CreateTables {
        CreateTables(SQLite::Database& db);
    } createTables;
    SQLite::Statement insertOffense;
    SQLite::Statement getOffenses;
    SQLite::Statement insertPeer;
    SQLite::Statement setlastseen;
    SQLite::Statement set_ws_lastseen;
    SQLite::Statement selectRecentPeers;
    SQLite::Statement insertClearBan;
    SQLite::Statement peerban;

    SQLite::Statement stmtResetBans;
    SQLite::Statement selectBan;
    SQLite::Statement selectCurrentBans;
    SQLite::Statement connection_log_insert;
    SQLite::Statement connection_log_update;
    SQLite::Statement refuseinsert;
};
