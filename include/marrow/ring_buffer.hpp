#pragma once

#include "marrow/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace marrow {

struct RingBufferConfig {
    int max_disk_bytes = 200 * 1024 * 1024;
    double retention_seconds = 30 * 60;
    int max_samples_per_series = 50000;
};

struct HistoryRange {
    double start = 0;
    double end = 0;
    static HistoryRange last_minutes(int minutes);
};

class IRingBufferStore {
public:
    virtual ~IRingBufferStore() = default;
    virtual bool open(const std::string& path) = 0;
    virtual void close() = 0;
    virtual bool append(const MetricsSnapshot& snapshot) = 0;
    virtual bool evict_if_needed() = 0;
    virtual int disk_usage_bytes() const = 0;
};

std::unique_ptr<IRingBufferStore> make_sqlite_ring_buffer(RingBufferConfig config = {});

}  // namespace marrow
