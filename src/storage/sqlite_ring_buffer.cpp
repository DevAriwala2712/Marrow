#include "sysscope/ring_buffer.hpp"
#include "sysscope/util.hpp"

#include <sqlite3.h>

#include <cstdio>
#include <memory>
#include <string>

#ifndef SQLITE_TRANSIENT
#define SQLITE_TRANSIENT reinterpret_cast<void(*)(void*)>(-1)
#endif

namespace sysscope {

HistoryRange HistoryRange::last_minutes(int minutes) {
    const double end = now_seconds();
    return {end - minutes * 60.0, end};
}

class SQLiteRingBufferStore : public IRingBufferStore {
public:
    explicit SQLiteRingBufferStore(RingBufferConfig cfg) : config_(cfg) {}

    bool open(const std::string& path) override {
        if (db_) return true;
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) return false;
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                kind TEXT NOT NULL,
                timestamp REAL NOT NULL,
                payload TEXT NOT NULL
            );
            CREATE INDEX IF NOT EXISTS idx_metrics_ts ON metrics(timestamp);
        )";
        return exec(sql);
    }

    void close() override {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }

    bool append(const MetricsSnapshot& snapshot) override {
        if (!db_) return false;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.6f", snapshot.timestamp);
        const std::string payload = std::string("ts=") + buf;  // stub serialization
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "INSERT INTO metrics (kind, timestamp, payload) VALUES ('cpu', ?, ?);";
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_double(stmt, 1, snapshot.timestamp);
        sqlite3_bind_text(stmt, 2, payload.c_str(), -1, SQLITE_TRANSIENT);
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        if (ok) evict_if_needed();
        return ok;
    }

    bool evict_if_needed() override {
        if (!db_) return false;
        const double cutoff = now_seconds() - config_.retention_seconds;
        char sql[128];
        std::snprintf(sql, sizeof(sql), "DELETE FROM metrics WHERE timestamp < %.6f;", cutoff);
        if (!exec(sql)) return false;
        while (disk_usage_bytes() > config_.max_disk_bytes) {
            if (!exec("DELETE FROM metrics WHERE id IN (SELECT id FROM metrics ORDER BY timestamp ASC LIMIT 1000);"))
                break;
            if (disk_usage_bytes() <= 0) break;
        }
        return true;
    }

    int disk_usage_bytes() const override {
        if (!db_) return 0;
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, "SELECT COALESCE(SUM(length(payload)),0) FROM metrics;", -1, &stmt, nullptr) != SQLITE_OK)
            return 0;
        int bytes = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) bytes = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return bytes;
    }

private:
    bool exec(const char* sql) {
        char* err = nullptr;
        const int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
        if (err) sqlite3_free(err);
        return rc == SQLITE_OK;
    }

    RingBufferConfig config_;
    sqlite3* db_ = nullptr;
};

std::unique_ptr<IRingBufferStore> make_sqlite_ring_buffer(RingBufferConfig config) {
    return std::make_unique<SQLiteRingBufferStore>(config);
}

}  // namespace sysscope
