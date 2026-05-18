#pragma once

#include "marrow/metric_provider.hpp"
#include "marrow/util.hpp"

#include <cmath>

namespace marrow {

class StubCpuProvider : public IMetricProvider {
public:
    MetricKind kind() const override { return MetricKind::Cpu; }
    void tick(MetricsSnapshot& s) override {
        phase_ += 0.15;
        const double base = 25.0 + std::sin(phase_) * 15.0;
        s.cpu.user_percent = base + rand_range(-3, 3);
        s.cpu.system_percent = 8.0 + rand_range(-2, 2);
        s.cpu.idle_percent = std::max(0.0, 100.0 - s.cpu.user_percent - s.cpu.system_percent);
        s.cpu.p_core_percent = base * 0.65;
        s.cpu.e_core_percent = base * 0.35;
        s.has_cpu = true;
    }
private:
    double phase_ = 0;
};

class StubMemoryProvider : public IMetricProvider {
public:
    MetricKind kind() const override { return MetricKind::Memory; }
    void tick(MetricsSnapshot& s) override {
        used_ = std::min<std::uint64_t>(
            total_ - 512ULL * 1024 * 1024,
            static_cast<std::uint64_t>(static_cast<std::int64_t>(used_) +
                static_cast<std::int64_t>(rand_range(-50e6, 80e6))));
        s.memory.used_bytes = used_;
        s.memory.total_bytes = total_;
        s.memory.wired_bytes = used_ / 4;
        s.memory.compressed_bytes = used_ / 8;
        s.has_memory = true;
    }
private:
    std::uint64_t total_ = 16ULL * 1024 * 1024 * 1024;
    std::uint64_t used_ = 10ULL * 1024 * 1024 * 1024;
};

class StubNetworkProvider : public IMetricProvider {
public:
    MetricKind kind() const override { return MetricKind::Network; }
    void tick(MetricsSnapshot& s) override {
        s.network.bytes_in_per_sec = rand_range(50e3, 500e3);
        s.network.bytes_out_per_sec = rand_range(20e3, 200e3);
        s.network.connections = {
            {1234, "Safari", "142.250.80.46", 443, 52431, "US", "HTTPS", 0, 0, false},
            {5678, "Cursor", "52.1.2.3", 443, 52432, "US", "HTTPS", 0, 0, false},
            {9012, "unknown", "185.220.101.1", 9050, 52433, "DE", "", 0, 0, true},
        };
        s.has_network = true;
    }
};

class StubDiskProvider : public IMetricProvider {
public:
    MetricKind kind() const override { return MetricKind::Disk; }
    void tick(MetricsSnapshot& s) override {
        static int idx = 0;
        const char* paths[] = {"/var/log/system.log", "/Users/dev/Library/Caches", "/tmp/marrow-stub"};
        const char* names[] = {"logd", "backupd", "find", "Marrow"};
        DiskIOEvent ev;
        ev.process_pid = static_cast<std::int32_t>(rand_range(100, 9999));
        ev.process_name = names[idx % 4];
        ev.path = paths[idx % 3];
        ev.bytes_read = static_cast<std::uint64_t>(rand_range(0, 1e6));
        ev.bytes_written = static_cast<std::uint64_t>(rand_range(0, 5e6));
        ev.latency_ms = rand_range(0.1, 50);
        ++idx;
        s.disk.read_bytes_per_sec = rand_range(0, 10e6);
        s.disk.write_bytes_per_sec = rand_range(0, 20e6);
        s.disk.recent_events = {ev};
        s.has_disk = true;
    }
};

class StubThermalProvider : public IMetricProvider {
public:
    MetricKind kind() const override { return MetricKind::Thermal; }
    void tick(MetricsSnapshot& s) override {
        t_ += 0.1;
        const double p = 40 + std::sin(t_) * 20;
        const double e = 25 + std::cos(t_ * 0.7) * 15;
        s.thermal.clusters = {
            {"P-Core", p, 55 + p * 0.15, p * 0.08},
            {"E-Core", e, 45 + e * 0.12, e * 0.04},
        };
        s.thermal.gpu_utilization_percent = 20 + std::sin(t_ * 1.2) * 15;
        s.thermal.ane_utilization_percent = 5 + std::cos(t_ * 0.5) * 5;
        s.thermal.die_temperature_grid.assign(8, std::vector<double>(8));
        for (int r = 0; r < 8; ++r)
            for (int c = 0; c < 8; ++c)
                s.thermal.die_temperature_grid[r][c] =
                    40 + std::sin(t_ + (r + c) * 0.3) * 12 + rand_range(-2, 2);
        s.has_thermal = true;
    }
private:
    double t_ = 0;
};

class StubProcessGraphProvider : public IMetricProvider {
public:
    MetricKind kind() const override { return MetricKind::ProcessGraph; }
    void tick(MetricsSnapshot& s) override {
        s.process_graph.nodes = {
            {1, "launchd", -1, 120, 4},
            {100, "WindowServer", 1, 340, 12},
            {200, "Marrow", 1, 45, 2},
            {201, "MarrowHelper", 200, 28, 1},
            {300, "Safari", 1, 890, 6},
            {301, "com.apple.WebKit", 300, 210, 3},
        };
        s.has_process_graph = true;
    }
};

}  // namespace marrow
