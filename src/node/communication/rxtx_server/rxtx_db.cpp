#include "rxtx_db.hpp"
#include "api_types.hpp"
#include "general/sqlite.hpp"
#include "global/globals.hpp"
#include "spdlog/spdlog.h"
namespace rxtx {

SqliteDB::SqliteDB(const std::string& path)
    : SQLite::Database(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)
{
    spdlog::debug("Opening RXTX database \"{}\"", path);

    exec("PRAGMA foreign_keys = ON;");
    exec("CREATE TABLE IF NOT EXISTS `hosts` ( `id` INTEGER, `host` TEXT UNIQUE, PRIMARY KEY(`id`))");
    exec("CREATE TABLE IF NOT EXISTS `hours` ( `timestamp` INTEGER, `host_id` BLOB, `rx` INTEGER, `tx` INTEGER, FOREIGN KEY(`host_id`) REFERENCES `hosts`(`id`) ON DELETE CASCADE)");
    exec("CREATE UNIQUE INDEX IF NOT EXISTS `hours_unique` ON `hours` ( `timestamp`, `host_id`)");
    exec("CREATE TABLE IF NOT EXISTS `minutes` ( `timestamp` INTEGER, `host_id` BLOB, `rx` INTEGER, `tx` INTEGER, FOREIGN KEY(`host_id`) REFERENCES `hosts`(`id`) ON DELETE CASCADE)");
    exec("CREATE UNIQUE INDEX IF NOT EXISTS `minutes_unique` ON `minutes` ( `timestamp`, `host_id`)");
}

SQLite::Transaction DB::create_transaction()
{
    return SQLite::Transaction(db);
}

void DB::insert_aggregated(const std::string& host, const RoundedTsAggregated<60 * 60>& a)
{
    int64_t id { host_id(host) };
    stmtInsertHour.run(a.end_time(), id);
    stmtAddHour.run(a.rx, a.tx, a.end_time(), id);
}

void DB::insert_aggregated(const std::string& host, const RoundedTsAggregated<60>& a)
{
    int64_t id { host_id(host) };
    stmtInsertMinute.run(a.end_time(), id);
    stmtAddMinute.run(a.rx, a.tx, a.end_time(), id);
}

int64_t DB::host_id(std::string host)
{
    stmtInsertHost.run(host);
    auto hostId { stmtSelectHost.one(host) };
    return hostId.get<int64_t>(0);
}
namespace {
    auto get_aggregated(auto& stmt, TimestampRange range, uint32_t bucketWidth)
    {
        api::TransmissionTimeseries res;
        stmt.for_each([&](sqlite::Row& row) {
            auto host(row.get<std::string>(0));
            Timestamp ts(row.get<int64_t>(1));
            size_t rx(row.get<int64_t>(2));
            size_t tx(row.get<int64_t>(3));
            res.byHost[host].push_back({ Timestamp(ts.val() - bucketWidth), ts, rx, tx });
        },
            range.begin, range.end);
        return res;
    }

}

api::TransmissionTimeseries DB::get_aggregated_minutes(TimestampRange range)
{
    return get_aggregated(stmtGetMinutes, range, 60);
}

api::TransmissionTimeseries DB::get_aggregated_hours(TimestampRange range)
{
    return get_aggregated(stmtGetHours, range, 60 * 60);
}

void DB::prune_minutes(Timestamp ts)
{
    stmtPruneMinute.run(ts);
}

void DB::prune_hours(Timestamp ts)
{
    stmtPruneHour.run(ts);
}

DB::DB()
    : db(config().data.rxtxdb)
    , stmtInsertMinute(db, "INSERT OR IGNORE INTO minutes (timestamp, host_id, rx, tx) VALUES (?,?,0,0)")
    , stmtInsertHour(db, "INSERT OR IGNORE INTO hours (timestamp, host_id, rx, tx) VALUES (?,?,0,0)")
    , stmtAddMinute(db, "UPDATE minutes SET `rx` = `rx` + ?, `tx` = `tx` + ? WHERE `timestamp` = ? AND `host_id` = ?")
    , stmtAddHour(db, "UPDATE hours SET `rx` = `rx` + ?, `tx` = `tx` + ? WHERE `timestamp` = ? AND `host_id` = ?")
    , stmtGetMinutes(db, "SELECT `host`, `timestamp`, `rx`, `tx` FROM minutes JOIN hosts ON host_id = id WHERE timestamp >= ? AND timestamp <= ?")
    , stmtGetHours(db, "SELECT `host`, `timestamp`,`rx`, `tx` FROM hours JOIN hosts ON host_id = id WHERE timestamp >= ? AND timestamp <= ?")
    , stmtPruneMinute(db, "DELETE FROM minutes WHERE timestamp <= ?")
    , stmtPruneHour(db, "DELETE FROM hours WHERE timestamp <= ?")
    , stmtInsertHost(db, "INSERT OR IGNORE INTO `hosts` (`host`) VALUES (?)")
    , stmtSelectHost(db, "SELECT `id` from `hosts` WHERE `host` = ?")
{
}
}
