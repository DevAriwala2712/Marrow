#include "sysscope/metrics_codec.hpp"

#include <json.hpp>

#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <utility>
#include <unistd.h>
#include <vector>

namespace sysscope {

using json = nlohmann::json;

std::string default_socket_path() {
    const char* home = std::getenv("HOME");
    if (!home) return "/tmp/sysscope-helper.sock";
    return std::string(home) + "/Library/Application Support/SysScope/helper.sock";
}

static json cpu_to_json(const CpuMetrics& c) {
    json j = {{"user", c.user_percent},
              {"system", c.system_percent},
              {"idle", c.idle_percent},
              {"p_core", c.p_core_percent},
              {"e_core", c.e_core_percent},
              {"per_core", c.per_core_percent}};
    return j;
}

static void cpu_from_json(const json& j, CpuMetrics& c) {
    c.user_percent = j.value("user", 0.0);
    c.system_percent = j.value("system", 0.0);
    c.idle_percent = j.value("idle", 0.0);
    c.p_core_percent = j.value("p_core", 0.0);
    c.e_core_percent = j.value("e_core", 0.0);
    if (j.contains("per_core")) c.per_core_percent = j["per_core"].get<std::vector<double>>();
}

static json grid_to_json(const std::vector<std::vector<double>>& grid) {
    json rows = json::array();
    for (const auto& row : grid) rows.push_back(row);
    return rows;
}

static void grid_from_json(const json& j, std::vector<std::vector<double>>& grid) {
    if (!j.is_array()) return;
    grid.clear();
    for (const auto& row_json : j) {
        if (!row_json.is_array()) continue;
        std::vector<double> row;
        for (const auto& cell : row_json) {
            if (cell.is_number()) row.push_back(cell.get<double>());
        }
        grid.push_back(std::move(row));
    }
}

static json snapshot_to_json(const MetricsSnapshot& s) {
    json j;
    j["timestamp"] = s.timestamp;
    if (s.has_cpu) j["cpu"] = cpu_to_json(s.cpu);
    if (s.has_memory) {
        j["memory"] = {{"used", s.memory.used_bytes},
                       {"total", s.memory.total_bytes},
                       {"wired", s.memory.wired_bytes},
                       {"compressed", s.memory.compressed_bytes},
                       {"cached", s.memory.cached_bytes},
                       {"free", s.memory.free_bytes}};
    }
    if (s.has_network) {
        j["network"] = {{"in_bps", s.network.bytes_in_per_sec},
                        {"out_bps", s.network.bytes_out_per_sec},
                        {"iface", s.network.interface_name}};
    }
    if (s.has_disk) {
        j["disk"] = {{"read_bps", s.disk.read_bytes_per_sec},
                     {"write_bps", s.disk.write_bytes_per_sec}};
    }
    if (s.has_thermal) {
        json clusters = json::array();
        for (const auto& c : s.thermal.clusters) {
            clusters.push_back({{"name", c.name},
                                {"util", c.utilization_percent},
                                {"temp", c.temperature_celsius},
                                {"watts", c.estimated_watts}});
        }
        j["thermal"] = {{"clusters", clusters},
                        {"gpu", s.thermal.gpu_utilization_percent},
                        {"ane", s.thermal.ane_utilization_percent},
                        {"cpu_temp", s.thermal.cpu_die_temp_celsius},
                        {"gpu_temp", s.thermal.gpu_die_temp_celsius},
                        {"fan_rpm", s.thermal.fan_rpm},
                        {"die_temperature_grid", grid_to_json(s.thermal.die_temperature_grid)}};
    }
    if (s.has_process_graph) {
        json procs = json::array();
        for (const auto& p : s.process_graph.nodes) {
            procs.push_back({{"pid", p.id},
                             {"name", p.name},
                             {"ppid", p.parent_pid},
                             {"cpu", p.cpu_percent},
                             {"mem", p.mem_bytes},
                             {"status", p.status}});
        }
        j["processes"] = procs;
    }
    return j;
}

static MetricsSnapshot snapshot_from_json(const json& j) {
    MetricsSnapshot s;
    s.timestamp = j.value("timestamp", 0.0);
    if (j.contains("cpu")) {
        cpu_from_json(j["cpu"], s.cpu);
        s.has_cpu = true;
    }
    if (j.contains("memory")) {
        const auto& m = j["memory"];
        s.memory.used_bytes = m.value("used", 0ULL);
        s.memory.total_bytes = m.value("total", 0ULL);
        s.memory.wired_bytes = m.value("wired", 0ULL);
        s.memory.compressed_bytes = m.value("compressed", 0ULL);
        s.memory.cached_bytes = m.value("cached", 0ULL);
        s.memory.free_bytes = m.value("free", 0ULL);
        s.has_memory = true;
    }
    if (j.contains("network")) {
        const auto& n = j["network"];
        s.network.bytes_in_per_sec = n.value("in_bps", 0.0);
        s.network.bytes_out_per_sec = n.value("out_bps", 0.0);
        s.network.interface_name = n.value("iface", "");
        s.has_network = true;
    }
    if (j.contains("disk")) {
        s.disk.read_bytes_per_sec = j["disk"].value("read_bps", 0.0);
        s.disk.write_bytes_per_sec = j["disk"].value("write_bps", 0.0);
        s.has_disk = true;
    }
    if (j.contains("thermal")) {
        const auto& t = j["thermal"];
        for (const auto& c : t.value("clusters", json::array())) {
            ClusterThermal cl;
            cl.name = c.value("name", "");
            cl.utilization_percent = c.value("util", 0.0);
            cl.temperature_celsius = c.value("temp", 0.0);
            cl.estimated_watts = c.value("watts", 0.0);
            s.thermal.clusters.push_back(cl);
        }
        s.thermal.gpu_utilization_percent = t.value("gpu", 0.0);
        s.thermal.ane_utilization_percent = t.value("ane", 0.0);
        s.thermal.cpu_die_temp_celsius = t.value("cpu_temp", 0.0);
        s.thermal.gpu_die_temp_celsius = t.value("gpu_temp", 0.0);
        s.thermal.fan_rpm = t.value("fan_rpm", -1);
        if (t.contains("die_temperature_grid")) {
            grid_from_json(t["die_temperature_grid"], s.thermal.die_temperature_grid);
        }
        s.has_thermal = true;
    }
    if (j.contains("processes")) {
        for (const auto& p : j["processes"]) {
            ProcessNode n;
            n.id = p.value("pid", 0);
            n.name = p.value("name", "");
            n.parent_pid = p.value("ppid", -1);
            n.cpu_percent = p.value("cpu", 0.0);
            n.mem_bytes = p.value("mem", 0ULL);
            n.status = p.value("status", "");
            s.process_graph.nodes.push_back(n);
        }
        s.has_process_graph = true;
    }
    return s;
}

std::string encode_request(const XpcRequest& req) {
    std::string kind;
    switch (req.kind) {
        case XpcRequestKind::Ping: kind = "ping"; break;
        case XpcRequestKind::FetchSnapshot: kind = "fetch_snapshot"; break;
        case XpcRequestKind::StartCollection: kind = "start"; break;
        case XpcRequestKind::StopCollection: kind = "stop"; break;
        default: kind = "ack"; break;
    }
    return json{{"kind", kind}}.dump();
}

std::string encode_response(const XpcResponse& resp) {
    json j;
    switch (resp.kind) {
        case XpcResponseKind::Pong:
            j["kind"] = "pong";
            j["version"] = resp.version;
            break;
        case XpcResponseKind::Snapshot:
            j["kind"] = "snapshot";
            j["snapshot"] = snapshot_to_json(resp.snapshot);
            break;
        default:
            j["kind"] = "ack";
            break;
    }
    return j.dump();
}

std::optional<XpcRequest> decode_request(const std::string& payload) {
    try {
        const json j = json::parse(payload);
        const std::string kind = j.value("kind", "");
        XpcRequest req;
        if (kind == "ping") req.kind = XpcRequestKind::Ping;
        else if (kind == "fetch_snapshot") req.kind = XpcRequestKind::FetchSnapshot;
        else if (kind == "start") req.kind = XpcRequestKind::StartCollection;
        else if (kind == "stop") req.kind = XpcRequestKind::StopCollection;
        else return std::nullopt;
        return req;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<XpcResponse> decode_response(const std::string& payload) {
    try {
        const json j = json::parse(payload);
        XpcResponse resp;
        const std::string kind = j.value("kind", "");
        if (kind == "pong") {
            resp.kind = XpcResponseKind::Pong;
            resp.version = j.value("version", "");
        } else if (kind == "snapshot") {
            resp.kind = XpcResponseKind::Snapshot;
            resp.snapshot = snapshot_from_json(j["snapshot"]);
        } else {
            resp.kind = XpcResponseKind::Ack;
        }
        return resp;
    } catch (...) {
        return std::nullopt;
    }
}

bool write_frame(int fd, const std::string& payload) {
    uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
    if (write(fd, &len, sizeof(len)) != static_cast<ssize_t>(sizeof(len))) return false;
    size_t off = 0;
    while (off < payload.size()) {
        const ssize_t n = write(fd, payload.data() + off, payload.size() - off);
        if (n <= 0) return false;
        off += static_cast<size_t>(n);
    }
    return true;
}

std::optional<std::string> read_frame(int fd) {
    uint32_t len_be = 0;
    ssize_t n = read(fd, &len_be, sizeof(len_be));
    if (n != static_cast<ssize_t>(sizeof(len_be))) return std::nullopt;
    const uint32_t len = ntohl(len_be);
    if (len == 0 || len > 16 * 1024 * 1024) return std::nullopt;
    std::string buf(len, '\0');
    size_t off = 0;
    while (off < len) {
        n = read(fd, buf.data() + off, len - off);
        if (n <= 0) return std::nullopt;
        off += static_cast<size_t>(n);
    }
    return buf;
}

}  // namespace sysscope
