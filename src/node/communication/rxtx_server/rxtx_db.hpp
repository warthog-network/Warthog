#pragma once
#include "aggregator.hpp"
#include "api_types_fwd.hpp"
#include "db/sqlite_fwd.hpp"
#include "general/timestamp.hpp"

class IP;
struct Peerhost;
namespace rxtx {

struct SqliteDB : public SQLite::Database {
    SqliteDB(const std::string& path);
};

class DB {
    using Statement = sqlite::Statement;

    [[nodiscard]] int64_t host_id(std::string host);

public:
    [[nodiscard]] SQLite::Transaction create_transaction();
    void insert_aggregated(const std::string& host, const HourAggregated&);
    void insert_aggregated(const std::string& host, const MinuteAggregated&);
    [[nodiscard]] api::TransmissionTimeseries get_aggregated_minutes(TimestampRange range);
    [[nodiscard]] api::TransmissionTimeseries get_aggregated_hours(TimestampRange range);
    void prune_minutes(Timestamp ts);
    void prune_hours(Timestamp ts);
    DB();

private:
    SqliteDB db;
    Statement stmtInsertMinute;
    Statement stmtInsertHour;
    Statement stmtAddMinute;
    Statement stmtAddHour;
    Statement stmtGetMinutes;
    Statement stmtGetHours;
    Statement stmtPruneMinute;
    Statement stmtPruneHour;
    Statement stmtInsertHost;
    Statement stmtSelectHost;
};
}
