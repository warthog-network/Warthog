#pragma once
#include "SQLiteCpp/SQLiteCpp.h"
#include "offense_entry.hpp"
#include "general/page.hpp"
#include "transport/helpers/tcp_sockaddr.hpp"
#include "general/now.hpp"
#include "general/errors.hpp"
#include <vector>

struct Sockaddr;
class PeerDB {
  private:
    // ids to save additional information in tables
    static constexpr int64_t WORKSUMID = -1;

  public:
    struct BanEntry {
            IPv4 ip;
            int32_t banuntil;
            Error offense;
            BanEntry( IPv4 ip, int32_t banuntil, uint32_t offense)
                :ip(ip),banuntil(banuntil),offense(offense){};

    };
    PeerDB(const std::string &path);
    SQLite::Transaction transaction() { return SQLite::Transaction(db); }
    void set_ban(IPv4 ipv4, uint32_t banUntil, int32_t offense) {
        peerset.bind(1, banUntil);
        peerset.bind(2, offense);
        peerset.bind(3, ipv4.data);
        peerset.exec();
        peerset.reset();
    }
    void insert_peer(IPv4 ipv4) {
        peerinsert.bind(1, ipv4.data);
        peerinsert.exec();
        peerinsert.reset();
    }

    std::vector<BanEntry> get_banned_peers(){
        std::vector<BanEntry> res;
        uint32_t t=now_timestamp();
        peergetBanned.bind(1,t);
        while (peergetBanned.executeStep()){
            IPv4 ip=uint32_t(peergetBanned.getColumn(0).getInt64());
            uint32_t banuntil=peergetBanned.getColumn(1).getInt64();
            int32_t offense=peergetBanned.getColumn(2).getInt64();
            BanEntry e(ip,banuntil,offense);
            res.push_back(e);
        }
        peergetBanned.reset();
        return res;
    }

    bool get_peer( IPv4 ipv4, uint32_t& banUntil, int32_t& offense){
        peerget.bind(1,ipv4.data);
        bool found=false;
        if (peerget.executeStep()){
            found=true;
            banUntil=peerget.getColumn(0).getInt64();
            offense=peerget.getColumn(1).getInt();
        }
        peerget.reset();
        return found;
    }

    int64_t insert_connect(uint32_t peer, uint32_t begin)
    {
        connectset.bind(1,peer);
        connectset.bind(2,begin);
        connectset.exec();
        connectset.reset();
        return db.getLastInsertRowid();
    }

    void insert_disconnect(int64_t rowid, uint32_t end, int32_t code){
        disconnectset.bind(1,end);
        disconnectset.bind(2,code);
        disconnectset.bind(3,rowid);
        disconnectset.exec();
        disconnectset.reset();
    }

    void insert_refuse(IPv4 ipv4, uint32_t timestamp)
    {
        refuseinsert.bind(1,ipv4.data);
        refuseinsert.bind(2,timestamp);
        refuseinsert.exec();
        refuseinsert.reset();
    }
    void insert_offense(IPv4 ipv4,int32_t offense){
        assert(offense!=0);
        insertOffense.bind(1,ipv4.data);
        insertOffense.bind(2,now_timestamp());
        insertOffense.bind(3,offense);
        insertOffense.exec();
        insertOffense.reset();
    }
    std::vector<OffenseEntry> get_offenses(Page page){
        assert(page.val()>0);
        std::vector<OffenseEntry> out;
        getOffenses.bind(1,(page.val()-1)*100);
        while (getOffenses.executeStep()){
            IPv4 ip=uint32_t(getOffenses.getColumn(0).getInt64());
            uint32_t timestamp=getOffenses.getColumn(1).getInt64();
            int32_t offense=getOffenses.getColumn(2).getInt64();
            out.push_back({ip,timestamp,offense});
        }
        getOffenses.reset();
        return out;
    }
    void reset_bans(){
        stmtResetBans.exec();
        stmtResetBans.reset();
    }
    std::vector<std::pair<TCPSockaddr,uint32_t>> recent_peers(int64_t maxEntries = 100);
    std::vector<std::pair<WSSockaddr,uint32_t>> recent_ws_peers(int64_t maxEntries = 100);
    void peer_seen(TCPSockaddr,uint32_t now);
    void ws_peer_seen(WSSockaddr a, uint32_t now);
    void peer_insert(TCPSockaddr);
    void ws_peer_insert(WSSockaddr);

  private:
    SQLite::Database db;
    struct CreateTables {
        CreateTables(SQLite::Database & db);
    } createTables;
    SQLite::Statement insertOffense;
    SQLite::Statement getOffenses;
    SQLite::Statement insertPeer;
    SQLite::Statement insertWsPeer;
    SQLite::Statement setlastseen;
    SQLite::Statement set_ws_lastseen;
    SQLite::Statement selectRecentPeers;
    SQLite::Statement selectRecentWsPeers;
    SQLite::Statement peerinsert;
    SQLite::Statement peerset;

    SQLite::Statement stmtResetBans;
    SQLite::Statement peerget;
    SQLite::Statement peergetBanned;
    SQLite::Statement connectset;
    SQLite::Statement disconnectset;
    SQLite::Statement refuseinsert;

};
