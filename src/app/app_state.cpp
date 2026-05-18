#include "sysscope/app_state.hpp"

#include "imgui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace sysscope {

AppState::AppState() {
    const auto dir = std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : "/tmp") /
                     "Library/Application Support/SysScope";
    std::filesystem::create_directories(dir);
    ring_buffer_ = make_sqlite_ring_buffer();
    ring_buffer_->open((dir / "history.sqlite").string());
}

void AppState::start() {
    aggregator_.set_providers(make_stub_providers());
    helper_.connect();
    helper_connected = true;
    helper_version = helper_.send({XpcRequestKind::Ping}).version;
}

void AppState::tick(double dt) {
    sample_accum_ += dt;
    if (sample_accum_ < 1.0) return;
    sample_accum_ = 0;
    snapshot = aggregator_.collect();
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
    ImGui::Text("Thermal & Power (stub)");
    if (!snapshot.has_thermal) return;
    for (const auto& cl : snapshot.thermal.clusters) {
        ImGui::BulletText("%s: %.0f%%  %.0f C  %.1f W", cl.name.c_str(), cl.utilization_percent,
                          cl.temperature_celsius, cl.estimated_watts);
    }
    ImGui::Text("GPU %.0f%%  ANE %.0f%%", snapshot.thermal.gpu_utilization_percent,
                snapshot.thermal.ane_utilization_percent);
    const auto& g = snapshot.thermal.die_temperature_grid;
    if (!g.empty() && !g[0].empty()) {
        const int rows = static_cast<int>(g.size());
        const int cols = static_cast<int>(g[0].size());
        double min_t = g[0][0], max_t = g[0][0];
        for (const auto& row : g)
            for (double v : row) {
                min_t = std::min(min_t, v);
                max_t = std::max(max_t, v);
            }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        const float cw = 18.f, ch = 18.f;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                const double norm = (g[r][c] - min_t) / std::max(max_t - min_t, 0.001);
                const ImU32 col = ImColor(static_cast<int>((1.0 - norm) * 80 + 100), 80,
                                            static_cast<int>(norm * 220 + 35));
                dl->AddRectFilled(ImVec2(p.x + c * cw, p.y + r * ch),
                                  ImVec2(p.x + (c + 1) * cw - 1, p.y + (r + 1) * ch - 1), col);
            }
        }
        ImGui::Dummy(ImVec2(cols * cw, rows * ch));
    }
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
