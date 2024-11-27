#pragma once
#include "SQLiteCpp/SQLiteCpp.h"
#include "general/error_time.hpp"
#include "general/errors.hpp"
#include "general/now.hpp"
#include "general/page.hpp"
#include "general/tcp_util.hpp"
#include "offense_entry.hpp"
#include <vector>

class PeerDB {
private:
    // ids to save additional information in tables
    static constexpr int64_t WORKSUMID = -1;

public:
    struct BanEntry {
        IPv4 ip;
        int32_t banuntil;
        Error offense;
        BanEntry(IPv4 ip, int32_t banuntil, uint32_t offense)
            : ip(ip)
            , banuntil(banuntil)
            , offense(offense) {};
    };
    PeerDB(const std::string& path);
    SQLite::Transaction transaction() { return SQLite::Transaction(db); }
    void set_ban(IPv4 ipv4,  ErrorTimestamp e)
    {
        peerset.bind(1, e.timestamp);
        peerset.bind(2, e.error.e);
        peerset.bind(3, ipv4.data);
        peerset.exec();
        peerset.reset();
    }
    void insert_peer(IPv4 ipv4)
    {
        peerinsert.bind(1, ipv4.data);
        peerinsert.exec();
        peerinsert.reset();
    }

    std::vector<BanEntry> get_banned_peers()
    {
        std::vector<BanEntry> res;
        uint32_t t = now_timestamp();
        peergetBanned.bind(1, t);
        while (peergetBanned.executeStep()) {
            IPv4 ip = uint32_t(peergetBanned.getColumn(0).getInt64());
            uint32_t banuntil = peergetBanned.getColumn(1).getInt64();
            int32_t offense = peergetBanned.getColumn(2).getInt64();
            BanEntry e(ip, banuntil, offense);
            res.push_back(e);
        }
        peergetBanned.reset();
        return res;
    }

    std::optional<ErrorTimestamp> get_ban_state(IPv4 ipv4)
    {
        std::optional<ErrorTimestamp> res;
        peerget.bind(1, ipv4.data);
        if (peerget.executeStep()) {
            auto reason { Error(peerget.getColumn(1).getInt()) };
            uint32_t expiresTimestamp ( peerget.getColumn(0).getInt64() );
            res = ErrorTimestamp{reason, expiresTimestamp};
        }
        peerget.reset();
        return res;
    }

    int64_t insert_connect(uint32_t peer, uint32_t begin)
    {
        connectset.bind(1, peer);
        connectset.bind(2, begin);
        connectset.exec();
        connectset.reset();
        return db.getLastInsertRowid();
    }

    void insert_disconnect(int64_t rowid, ErrorTimestamp et)
    {
        disconnectset.bind(1, et.timestamp);
        disconnectset.bind(2, et.error.e);
        disconnectset.bind(3, rowid);
        disconnectset.exec();
        disconnectset.reset();
    }

    void insert_refuse(IPv4 ipv4, uint32_t timestamp)
    {
        refuseinsert.bind(1, ipv4.data);
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
    std::vector<OffenseEntry> get_offenses(Page page)
    {
        assert(page.val() > 0);
        std::vector<OffenseEntry> out;
        getOffenses.bind(1, (page.val() - 1) * 100);
        while (getOffenses.executeStep()) {
            IPv4 ip = uint32_t(getOffenses.getColumn(0).getInt64());
            uint32_t timestamp = getOffenses.getColumn(1).getInt64();
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
    std::vector<std::pair<EndpointAddress, uint32_t>> recent_peers(int64_t maxEntries = 100);
    void peer_seen(EndpointAddress, uint32_t now);
    void peer_insert(EndpointAddress);

private:
    SQLite::Database db;
    struct CreateTables {
        CreateTables(SQLite::Database& db);
    } createTables;
    SQLite::Statement insertOffense;
    SQLite::Statement getOffenses;
    SQLite::Statement insertPeer;
    SQLite::Statement setlastseen;
    SQLite::Statement selectRecentPeers;
    SQLite::Statement peerinsert;
    SQLite::Statement peerset;
    SQLite::Statement stmtResetBans;
    SQLite::Statement peerget;
    SQLite::Statement peergetBanned;
    SQLite::Statement connectset;
    SQLite::Statement disconnectset;
    SQLite::Statement refuseinsert;
};
