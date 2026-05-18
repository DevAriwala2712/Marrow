#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sysscope {

enum class MetricKind {
    Cpu,
    Memory,
    Network,
    Disk,
    Thermal,
    ProcessGraph
};

struct CpuMetrics {
    double user_percent = 0;
    double system_percent = 0;
    double idle_percent = 0;
    double p_core_percent = 0;
    double e_core_percent = 0;
    double total_used() const { return user_percent + system_percent; }
};

struct MemoryMetrics {
    std::uint64_t used_bytes = 0;
    std::uint64_t total_bytes = 0;
    std::uint64_t wired_bytes = 0;
    std::uint64_t compressed_bytes = 0;
    double used_percent() const {
        return total_bytes ? (100.0 * used_bytes / total_bytes) : 0.0;
    }
};

struct NetworkConnection {
    std::int32_t process_pid = 0;
    std::string process_name;
    std::string remote_host;
    std::uint16_t remote_port = 0;
    std::uint16_t local_port = 0;
    std::string country_code;
    std::string service_name;
    std::uint64_t bytes_in = 0;
    std::uint64_t bytes_out = 0;
    bool is_anomaly = false;
};

struct NetworkMetrics {
    double bytes_in_per_sec = 0;
    double bytes_out_per_sec = 0;
    std::vector<NetworkConnection> connections;
};

struct DiskIOEvent {
    std::int32_t process_pid = 0;
    std::string process_name;
    std::string path;
    std::uint64_t bytes_read = 0;
    std::uint64_t bytes_written = 0;
    double latency_ms = 0;
};

struct DiskMetrics {
    double read_bytes_per_sec = 0;
    double write_bytes_per_sec = 0;
    std::vector<DiskIOEvent> recent_events;
};

struct ClusterThermal {
    std::string name;
    double utilization_percent = 0;
    double temperature_celsius = 0;
    double estimated_watts = 0;
};

struct ThermalMetrics {
    std::vector<ClusterThermal> clusters;
    double gpu_utilization_percent = 0;
    double ane_utilization_percent = 0;
    std::vector<std::vector<double>> die_temperature_grid;
};

struct ProcessNode {
    std::int32_t id = 0;
    std::string name;
    std::int32_t parent_pid = -1;
    int open_fd_count = 0;
    int ipc_connection_count = 0;
};

struct ProcessGraphSnapshot {
    std::vector<ProcessNode> nodes;
};

struct MetricsSnapshot {
    double timestamp = 0;
    bool has_cpu = false;
    bool has_memory = false;
    bool has_network = false;
    bool has_disk = false;
    bool has_thermal = false;
    bool has_process_graph = false;
    CpuMetrics cpu;
    MemoryMetrics memory;
    NetworkMetrics network;
    DiskMetrics disk;
    ThermalMetrics thermal;
    ProcessGraphSnapshot process_graph;
};

}  // namespace sysscope
