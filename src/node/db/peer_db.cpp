#include "peer_db.hpp"
#include "general/now.hpp"
PeerDB::CreateTables::CreateTables(SQLite::Database& db)
{
    db.exec("CREATE TABLE IF NOT EXISTS \"offenses\" ( \"ip\"	INTEGER, \"timestamp\", \"offense\"	TEXT)");
    db.exec("CREATE TABLE IF NOT EXISTS \"bans\" ( `ip` INTEGER NOT NULL, "
            "`ban_until` INTEGER NOT NULL DEFAULT 0, `offense` INTEGER "
            "NOT NULL, PRIMARY KEY(`ip`) )");
    db.exec("CREATE TABLE IF NOT EXISTS `connection_log` ( `peer` INTEGER NOT NULL, "
            "`begin` INTEGER NOT NULL, `end` INTEGER DEFAULT NULL, "
            "`code` INTEGER DEFAULT NULL )");
    db.exec("CREATE TABLE IF NOT EXISTS `refuse_log` ( `peer` INTEGER NOT NULL, `timestamp` INTEGER NOT NULL )");
    db.exec("CREATE INDEX IF NOT EXISTS `bans_index` ON `bans` ( `ban_until` DESC )");
    db.exec(R"SQL(CREATE TABLE IF NOT EXISTS "peers" ( "ipport" INTEGER, "lastseen" INTEGER DEFAULT 0, PRIMARY KEY("ipport")))SQL");
    db.exec(R"SQL(CREATE INDEX IF NOT EXISTS "lastseen_peers" ON "peers" ( "lastseen"))SQL");
}

PeerDB::PeerDB(const std::string& path)
    : db(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)
    , createTables(db)
    , insertOffense(db, "INSERT INTO `offenses` (`ip`, `timestamp`, `offense`) VALUES (?,?,?) ")
    , getOffenses(db, "SELECT `ip`, `timestamp`, `offense` FROM `offenses` LIMIT 100 OFFSET ?")
    , insertPeer(db, "INSERT OR IGNORE INTO `peers` (`ipport`) VALUES (?) ")
    , setlastseen(db, "UPDATE `peers` SET `lastseen`=? WHERE `ipport`=?")
    , selectRecentPeers(db, "SELECT `ipport`, `lastseen` FROM `peers` ORDER BY `lastseen` DESC LIMIT ?")

    , peerinsert(db, "INSERT INTO `bans` (`ip`,`ban_until`,`offense`) VALUES "
                     "(?,0,0)")
    , peerset(db, "UPDATE `bans` SET `ban_until`=?, `offense`=? WHERE `ip`=?")
    , stmtResetBans(db, "UPDATE `bans` SET `ban_until`=0, `offense`=0")

    , peerget(db, "SELECT `ban_until`, `offense` FROM `bans` WHERE `ip`=?")
    , peergetBanned(db, "SELECT `ip`,`ban_until`, `offense` FROM `bans` WHERE `ban_until`>?")
    , connectset(db, "INSERT INTO `connection_log` (`peer`,`begin`) VALUES "
                     "(?,?)")
    , disconnectset(db, "UPDATE `connection_log` SET `end`=?, `code`=? WHERE ROWID=?")
    , refuseinsert(db, "INSERT INTO `refuse_log` (`peer`,`timestamp`) VALUES (?,?)")
{
}

std::vector<std::pair<EndpointAddress, uint32_t>> PeerDB::recent_peers(int64_t maxEntries)
{
    std::vector<std::pair<EndpointAddress, uint32_t>> out;
    selectRecentPeers.bind(1, maxEntries);
    while (selectRecentPeers.executeStep()) {
        int64_t id = selectRecentPeers.getColumn(0).getInt64();
        uint32_t timestamp = selectRecentPeers.getColumn(1).getInt64();
        out.push_back({ EndpointAddress::from_sql_id(id), timestamp });
    }
    selectRecentPeers.reset();
    return out;
};

void PeerDB::peer_seen(EndpointAddress a, uint32_t now)
{
    setlastseen.bind(1, now);
    setlastseen.bind(2, a.to_sql_id());
    setlastseen.exec();
    setlastseen.reset();
};
void PeerDB::peer_insert(EndpointAddress a)
{
    insertPeer.bind(1, a.to_sql_id());
    insertPeer.exec();
    insertPeer.reset();
};
