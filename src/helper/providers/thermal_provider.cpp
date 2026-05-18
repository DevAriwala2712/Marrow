#include "marrow/providers/thermal_provider.hpp"

#include <json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <unistd.h>

namespace marrow {

using json = nlohmann::json;

namespace {

struct HeatSource {
    double row = 0;
    double col = 0;
    double temperature = 0;
    double weight = 1;
};

static std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

static bool contains_any(const std::string& text, std::initializer_list<const char*> needles) {
    for (const char* needle : needles) {
        if (text.find(needle) != std::string::npos) return true;
    }
    return false;
}

static std::vector<std::vector<double>> make_fallback_grid(const ThermalMetrics& m) {
    constexpr int kSize = 8;
    std::vector<std::vector<double>> grid(kSize, std::vector<double>(kSize, 0.0));

    double cpu_temp = m.cpu_die_temp_celsius > 0 ? m.cpu_die_temp_celsius : 54.0;
    double gpu_temp = m.gpu_die_temp_celsius > 0 ? m.gpu_die_temp_celsius : cpu_temp - 4.0;
    double p_temp = cpu_temp;
    double e_temp = std::max(40.0, cpu_temp - 8.0);
    double cpu_util = 0.0;
    double p_util = 0.0;
    double e_util = 0.0;

    for (const auto& cluster : m.clusters) {
        const std::string name = to_lower(cluster.name);
        if (contains_any(name, {"p-core", "performance", "p cluster"})) {
            p_temp = cluster.temperature_celsius > 0 ? cluster.temperature_celsius : p_temp;
            p_util = cluster.utilization_percent;
        } else if (contains_any(name, {"e-core", "efficiency", "e cluster"})) {
            e_temp = cluster.temperature_celsius > 0 ? cluster.temperature_celsius : e_temp;
            e_util = cluster.utilization_percent;
        } else if (contains_any(name, {"cpu", "die"})) {
            cpu_temp = cluster.temperature_celsius > 0 ? cluster.temperature_celsius : cpu_temp;
            cpu_util = cluster.utilization_percent;
        }
    }

    const double ane_temp = std::max(38.0, cpu_temp - 12.0);
    const double base = std::max({34.0, cpu_temp - 12.0, gpu_temp - 10.0, p_temp - 15.0, e_temp - 15.0});
    const std::vector<HeatSource> sources = {
        {1.5, 1.5, p_temp + p_util * 0.04, 1.30},
        {5.5, 5.8, e_temp + e_util * 0.04, 1.00},
        {3.5, 3.5, cpu_temp + cpu_util * 0.03, 1.10},
        {2.0, 5.5, gpu_temp, 1.15},
        {6.0, 3.5, ane_temp + m.ane_utilization_percent * 0.03, 0.85},
    };

    for (int r = 0; r < kSize; ++r) {
        for (int c = 0; c < kSize; ++c) {
            double value = base + 0.35 * std::sin((r + 1) * 0.55 + (c + 1) * 0.30);
            value += 0.15 * static_cast<double>(r) + 0.08 * static_cast<double>(c);
            for (const auto& src : sources) {
                const double dr = static_cast<double>(r) - src.row;
                const double dc = static_cast<double>(c) - src.col;
                const double influence = std::exp(-(dr * dr + dc * dc) / 4.5);
                value += (src.temperature - base) * src.weight * influence * 0.22;
            }
            grid[r][c] = std::clamp(value, base - 2.0, base + 28.0);
        }
    }

    return grid;
}

}  // namespace

void ThermalProvider::start() {
    if (running_.exchange(true)) return;
    reader_ = std::thread([this] { reader_loop(); });
}

void ThermalProvider::stop() {
    running_ = false;
    if (reader_.joinable()) reader_.join();
}

void ThermalProvider::tick(MetricsSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!has_data_) {
        snapshot.thermal.clusters = {
            {"P-Core", 40.0, 62.0, 3.0},
            {"E-Core", 24.0, 52.0, 1.2},
        };
        snapshot.thermal.gpu_utilization_percent = 18.0;
        snapshot.thermal.ane_utilization_percent = 6.0;
        snapshot.thermal.cpu_die_temp_celsius = 54.0;
        snapshot.thermal.gpu_die_temp_celsius = 50.0;
        snapshot.thermal.die_temperature_grid = make_fallback_grid(snapshot.thermal);
        snapshot.has_thermal = true;
        return;
    }
    snapshot.thermal = latest_;
    if (snapshot.thermal.die_temperature_grid.empty()) {
        snapshot.thermal.die_temperature_grid = make_fallback_grid(snapshot.thermal);
    }
    snapshot.has_thermal = true;
}

void ThermalProvider::apply_json_line(const std::string& line) {
    if (line.empty() || line[0] != '{') return;
    json root;
    try {
        root = json::parse(line);
    } catch (...) {
        return;
    }

    ThermalMetrics m;

    auto set_cluster = [&](const char* key, const char* label) {
        if (!root.contains(key)) return;
        const auto& v = root[key];
        ClusterThermal c;
        c.name = label;
        if (v.is_object()) {
            if (v.contains("active_ratio")) c.utilization_percent = v["active_ratio"].get<double>() * 100.0;
            if (v.contains("active_residency")) c.utilization_percent = v["active_residency"].get<double>();
        }
        m.clusters.push_back(c);
    };

    set_cluster("E-Cluster", "E-Core");
    set_cluster("P-Cluster", "P-Core");
    set_cluster("CPU", "CPU");

    if (root.contains("ane_energy")) {
        const auto& ane = root["ane_energy"];
        if (ane.contains("active_ratio")) m.ane_utilization_percent = ane["active_ratio"].get<double>() * 100.0;
    }
    if (root.contains("gpu_energy")) {
        const auto& gpu = root["gpu_energy"];
        if (gpu.contains("active_ratio")) m.gpu_utilization_percent = gpu["active_ratio"].get<double>() * 100.0;
    }
    if (root.contains("thermal_pressure")) {
        const auto& tp = root["thermal_pressure"];
        if (tp.contains("cpu")) m.cpu_die_temp_celsius = tp["cpu"].get<double>();
        if (tp.contains("gpu")) m.gpu_die_temp_celsius = tp["gpu"].get<double>();
    }
    if (root.contains("fan")) {
        const auto& fan = root["fan"];
        if (fan.is_number_integer()) m.fan_rpm = fan.get<int>();
        else if (fan.is_array() && !fan.empty() && fan[0].contains("speed"))
            m.fan_rpm = fan[0]["speed"].get<int>();
    }

    for (auto it = root.begin(); it != root.end(); ++it) {
        const std::string k = it.key();
        if (k.find("temp") != std::string::npos && it.value().is_number()) {
            if (m.cpu_die_temp_celsius <= 0) m.cpu_die_temp_celsius = it.value().get<double>();
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    latest_ = m;
    has_data_ = true;
}

void ThermalProvider::reader_loop() {
    FILE* pipe = popen("/usr/bin/powermetrics -i 1000 -f json -n 0 --samplers cpu_power,gpu_power,ane_power 2>/dev/null",
                       "r");
    if (!pipe) return;

    char buf[65536];
    std::string acc;
    while (running_ && fgets(buf, sizeof(buf), pipe)) {
        acc += buf;
        if (acc.find('}') != std::string::npos) {
            apply_json_line(acc);
            acc.clear();
        }
    }
    pclose(pipe);
}

}  // namespace marrow
