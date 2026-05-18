#include "sysscope/app_state.hpp"

#include "imgui/imgui.h"

#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <vector>

namespace sysscope {

namespace {

static ImU32 thermal_color(double norm) {
    norm = std::clamp(norm, 0.0, 1.0);
    const ImVec4 color = ImColor::HSV(static_cast<float>((1.0 - norm) * 0.68), 0.9f, 0.98f);
    return ImGui::ColorConvertFloat4ToU32(color);
}

static void draw_metric_card(const char* label, const std::string& value, const ImVec4& tint) {
    ImGui::BeginGroup();
    ImGui::TextColored(tint, "%s", label);
    ImGui::TextUnformatted(value.c_str());
    ImGui::EndGroup();
}

static void draw_heatmap_grid(const std::vector<std::vector<double>>& grid) {
    size_t rows = grid.size();
    size_t cols = 0;
    size_t value_count = 0;
    double min_t = std::numeric_limits<double>::max();
    double max_t = std::numeric_limits<double>::lowest();
    double sum_t = 0.0;

    for (const auto& row : grid) {
        cols = std::max(cols, row.size());
        for (double value : row) {
            min_t = std::min(min_t, value);
            max_t = std::max(max_t, value);
            sum_t += value;
            ++value_count;
        }
    }

    if (rows == 0 || cols == 0 || value_count == 0) {
        ImGui::TextDisabled("Heatmap is waiting for temperature samples.");
        return;
    }

    ImGui::Text("Grid %zux%zu  min %.1f C  max %.1f C  avg %.1f C",
                rows, cols, min_t, max_t, sum_t / static_cast<double>(value_count));
    ImGui::Spacing();

    const float avail_width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float cell_size = std::clamp(avail_width / static_cast<float>(cols), 15.0f, 30.0f);
    const float heatmap_width = cell_size * static_cast<float>(cols);
    const float heatmap_height = cell_size * static_cast<float>(rows);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImU32 background = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);

    draw_list->AddRectFilled(origin, ImVec2(origin.x + heatmap_width, origin.y + heatmap_height), background, 8.0f);

    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < grid[r].size(); ++c) {
            const double value = grid[r][c];
            const double norm = max_t > min_t ? (value - min_t) / (max_t - min_t) : 0.5;
            const ImVec2 cell_min(origin.x + static_cast<float>(c) * cell_size + 1.0f,
                                  origin.y + static_cast<float>(r) * cell_size + 1.0f);
            const ImVec2 cell_max(cell_min.x + cell_size - 2.0f, cell_min.y + cell_size - 2.0f);
            draw_list->AddRectFilled(cell_min, cell_max, thermal_color(norm), 4.0f);
            draw_list->AddRect(cell_min, cell_max, border, 4.0f);
        }
    }

    ImGui::Dummy(ImVec2(heatmap_width, heatmap_height));
    ImGui::Spacing();

    const float legend_width = std::min(heatmap_width, std::max(180.0f, heatmap_width));
    const float legend_height = 12.0f;
    const ImVec2 legend_origin = ImGui::GetCursorScreenPos();
    const int segments = 24;
    for (int i = 0; i < segments; ++i) {
        const float x0 = legend_origin.x + (legend_width * i) / segments;
        const float x1 = legend_origin.x + (legend_width * (i + 1)) / segments;
        draw_list->AddRectFilled(ImVec2(x0, legend_origin.y), ImVec2(x1, legend_origin.y + legend_height),
                                 thermal_color(static_cast<double>(i) / (segments - 1)));
    }
    draw_list->AddRect(legend_origin,
                       ImVec2(legend_origin.x + legend_width, legend_origin.y + legend_height), border, 4.0f);
    ImGui::Dummy(ImVec2(legend_width, legend_height));
    ImGui::TextDisabled("Cold -> Hot");
}

}  // namespace

AppState::AppState() {
    const auto dir = std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : "/tmp") /
                     "Library/Application Support/SysScope";
    std::filesystem::create_directories(dir);
    ring_buffer_ = make_sqlite_ring_buffer();
    ring_buffer_->open((dir / "history.sqlite").string());
}

void AppState::start() {
    helper_.connect();
    helper_connected = true;
    if (helper_.use_stubs) {
        aggregator_.set_providers(make_stub_providers());
    }
    helper_version = helper_.send({XpcRequestKind::Ping}).version;
}

void AppState::tick(double dt) {
    sample_accum_ += dt;
    if (sample_accum_ < 1.0) return;
    sample_accum_ = 0;
    if (helper_connected && !helper_.use_stubs) {
        const auto resp = helper_.send({XpcRequestKind::FetchSnapshot});
        if (resp.kind == XpcResponseKind::Snapshot) snapshot = resp.snapshot;
    } else {
        snapshot = aggregator_.collect();
    }
    if (snapshot.has_cpu) {
        cpu_history.push_back(static_cast<float>(snapshot.cpu.total_used()));
        while (cpu_history.size() > 120) cpu_history.pop_front();
    }
    ring_buffer_->append(snapshot);
}

void AppState::draw_sparkline(const char* id, const std::deque<float>& data, float r, float g, float b) {
    if (data.empty()) return;
    std::vector<float> buf(data.begin(), data.end());
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(r, g, b, 1.f));
    ImGui::PlotLines(id, buf.data(), static_cast<int>(buf.size()), 0, nullptr, 0.f, 100.f, ImVec2(80, 28));
    ImGui::PopStyleColor();
}

void AppState::draw_sidebar() {
    ImGui::BeginChild("sidebar", ImVec2(200, 0), true);
    ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.f, 1.f), "SysScope");
    ImGui::Separator();
    for (int i = 0; i < static_cast<int>(AppSection::COUNT); ++i) {
        auto sec = static_cast<AppSection>(i);
        if (ImGui::Selectable(section_name(sec), section == sec)) section = sec;
    }
    ImGui::EndChild();
    ImGui::SameLine();
}

void AppState::draw_process() {
    ImGui::Text("Process Lineage (stub)");
    ImGui::TextDisabled("Force-directed graph · FDs · IPC · syscall history");
    if (!snapshot.has_process_graph) {
        ImGui::Text("Waiting for process graph...");
        return;
    }
    if (ImGui::BeginTable("proc", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("PID");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Parent");
        ImGui::TableSetupColumn("FD/IPC");
        ImGui::TableHeadersRow();
        for (const auto& n : snapshot.process_graph.nodes) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%d", n.id);
            ImGui::TableNextColumn();
            ImGui::Text("%s", n.name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", n.parent_pid);
            ImGui::TableNextColumn();
            ImGui::Text("%d / %d", n.open_fd_count, n.ipc_connection_count);
        }
        ImGui::EndTable();
    }
}

void AppState::draw_network() {
    ImGui::Text("Network Topology (stub)");
    if (!snapshot.has_network) return;
    ImGui::Text("In: %.0f B/s  Out: %.0f B/s", snapshot.network.bytes_in_per_sec,
                snapshot.network.bytes_out_per_sec);
    if (ImGui::BeginTable("net", 4, ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Process");
        ImGui::TableSetupColumn("Remote");
        ImGui::TableSetupColumn("CC");
        ImGui::TableSetupColumn("!");
        ImGui::TableHeadersRow();
        for (const auto& c : snapshot.network.connections) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", c.process_name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%s:%u", c.remote_host.c_str(), c.remote_port);
            ImGui::TableNextColumn();
            ImGui::Text("%s", c.country_code.c_str());
            ImGui::TableNextColumn();
            if (c.is_anomaly) ImGui::TextColored(ImVec4(1, .6f, .2f, 1), "ANOMALY");
        }
        ImGui::EndTable();
    }
}

void AppState::draw_thermal() {
    ImGui::Text("Thermal & Power");
    ImGui::TextDisabled("Die temperatures, cluster load, and the active heatmap");
    if (!snapshot.has_thermal) {
        ImGui::Spacing();
        ImGui::TextDisabled("Waiting for thermal samples...");
        return;
    }

    if (ImGui::BeginTable("thermal_cards", 4,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadInnerX)) {
        ImGui::TableSetupColumn("CPU die");
        ImGui::TableSetupColumn("GPU die");
        ImGui::TableSetupColumn("GPU util");
        ImGui::TableSetupColumn("Fan");
        ImGui::TableNextColumn();
        draw_metric_card("CPU die",
                         std::to_string(static_cast<int>(snapshot.thermal.cpu_die_temp_celsius)) + " C",
                         ImVec4(0.95f, 0.60f, 0.30f, 1.0f));
        ImGui::TableNextColumn();
        draw_metric_card("GPU die",
                         std::to_string(static_cast<int>(snapshot.thermal.gpu_die_temp_celsius)) + " C",
                         ImVec4(0.85f, 0.55f, 0.95f, 1.0f));
        ImGui::TableNextColumn();
        draw_metric_card("GPU util",
                         std::to_string(static_cast<int>(snapshot.thermal.gpu_utilization_percent)) + "%",
                         ImVec4(0.35f, 0.75f, 1.0f, 1.0f));
        ImGui::TableNextColumn();
        if (snapshot.thermal.fan_rpm > 0) {
            draw_metric_card("Fan", std::to_string(snapshot.thermal.fan_rpm) + " RPM",
                             ImVec4(0.55f, 0.95f, 0.70f, 1.0f));
        } else {
            draw_metric_card("Fan", "n/a", ImVec4(0.65f, 0.65f, 0.65f, 1.0f));
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!snapshot.thermal.clusters.empty()) {
        ImGui::Text("Clusters");
        for (const auto& cl : snapshot.thermal.clusters) {
            ImGui::BulletText("%s: %.0f%%  %.0f C  %.1f W", cl.name.c_str(), cl.utilization_percent,
                              cl.temperature_celsius, cl.estimated_watts);
        }
        ImGui::Spacing();
    }

    ImGui::Text("Accelerators: GPU %.0f%%  ANE %.0f%%", snapshot.thermal.gpu_utilization_percent,
                snapshot.thermal.ane_utilization_percent);
    ImGui::Spacing();
    draw_heatmap_grid(snapshot.thermal.die_temperature_grid);
}

void AppState::draw_disk() {
    ImGui::Text("Disk I/O Forensics (stub)");
    if (!snapshot.has_disk) return;
    ImGui::Text("Read %.0f B/s  Write %.0f B/s", snapshot.disk.read_bytes_per_sec,
                snapshot.disk.write_bytes_per_sec);
    for (const auto& e : snapshot.disk.recent_events) {
        ImGui::BulletText("%s  %s", e.process_name.c_str(), e.path.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("R:%llu W:%llu %.1fms", e.bytes_read, e.bytes_written, e.latency_ms);
    }
}

void AppState::draw_timeline() {
    ImGui::Text("Timeline Replay (stub) — 30 min ring buffer");
    ImGui::SliderFloat("Scrub", &timeline_scrub, 0.f, 1.f);
    if (!cpu_history.empty()) {
        std::vector<float> buf(cpu_history.begin(), cpu_history.end());
        ImGui::PlotLines("CPU history", buf.data(), static_cast<int>(buf.size()), 0, nullptr, 0.f,
                         100.f, ImVec2(-1, 80));
    }
    ImGui::TextDisabled("Ring buffer: %d bytes on disk", ring_buffer_->disk_usage_bytes());
}

void AppState::draw_settings() {
    ImGui::Text("Settings");
    ImGui::BeginDisabled();
    ImGui::Checkbox("Use stub providers (scaffold)", &use_stub_providers);
    ImGui::EndDisabled();
    ImGui::Text("Helper: %s", helper_connected ? "connected" : "offline");
    ImGui::Text("Version: %s", helper_version.c_str());
    ImGui::Separator();
    ImGui::Text("Retention: 30 min  |  Disk cap: 200 MB");
}

void AppState::draw() {
    draw_sidebar();
    ImGui::BeginChild("content", ImVec2(0, 0), true);
    ImGui::TextColored(ImVec4(0.9f, 0.95f, 1.f, 1.f), "%s", section_name(section));
    ImGui::Separator();
    switch (section) {
        case AppSection::Process: draw_process(); break;
        case AppSection::Network: draw_network(); break;
        case AppSection::Thermal: draw_thermal(); break;
        case AppSection::Disk: draw_disk(); break;
        case AppSection::Timeline: draw_timeline(); break;
        case AppSection::Settings: draw_settings(); break;
        default: break;
    }
    ImGui::EndChild();
}

}  // namespace sysscope
