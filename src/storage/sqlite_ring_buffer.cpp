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

class SQLiteRingBufferStore : public IRingBufferStore {
public:
    explicit SQLiteRingBufferStore(RingBufferConfig cfg) : config_(cfg) {}

    ~SQLiteRingBufferStore() override { close(); }

    bool open(const std::string& path) override {
        if (db_) return true;
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
            close();
            return false;
        }
        sqlite3_busy_timeout(db_, 250);
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS metrics (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                kind TEXT NOT NULL,
                timestamp REAL NOT NULL,
                payload TEXT NOT NULL
            );
            CREATE INDEX IF NOT EXISTS idx_metrics_ts ON metrics(timestamp);
        )";
        if (exec(sql)) return true;
        close();
        return false;
    }

    void close() override {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }

    bool append(const MetricsSnapshot& snapshot) override {
        if (!db_) return false;
        if (!snapshot.has_cpu) return true;
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "INSERT INTO metrics (kind, timestamp, payload) VALUES ('cpu_percent', ?, ?);";
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_double(stmt, 1, snapshot.timestamp);
        sqlite3_bind_double(stmt, 2, snapshot.cpu.total_used());
        const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        return ok && evict_if_needed();
    }

    std::vector<CpuSample> cpu_history(double since) const override {
        if (!db_) return {};
        sqlite3_stmt* stmt = nullptr;
        const char* sql = R"(
            SELECT timestamp, payload FROM (
                SELECT timestamp, payload FROM metrics
                WHERE kind = 'cpu_percent' AND timestamp >= ?
                ORDER BY timestamp DESC LIMIT ?
            ) ORDER BY timestamp
        )";
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return {};
        sqlite3_bind_double(stmt, 1, since);
        sqlite3_bind_int(stmt, 2, config_.max_samples_per_series);
        std::vector<CpuSample> samples;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            samples.push_back({sqlite3_column_double(stmt, 0),
                               static_cast<float>(sqlite3_column_double(stmt, 1))});
        }
        sqlite3_finalize(stmt);
        return samples;
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
