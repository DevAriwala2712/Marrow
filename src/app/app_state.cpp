#include "marrow/app_state.hpp"

#include "imgui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace marrow {

namespace {

constexpr ImVec4 kBgSidebar = ImVec4(10.0f / 255.0f, 10.0f / 255.0f, 10.0f / 255.0f, 1.0f);
constexpr ImVec4 kTextPrimary = ImVec4(224.0f / 255.0f, 224.0f / 255.0f, 224.0f / 255.0f, 1.0f);
constexpr ImVec4 kTextMuted = ImVec4(85.0f / 255.0f, 85.0f / 255.0f, 85.0f / 255.0f, 1.0f);
constexpr ImVec4 kBorderColor = ImVec4(34.0f / 255.0f, 34.0f / 255.0f, 34.0f / 255.0f, 1.0f);
constexpr ImVec4 kAccentColor = ImVec4(0.0f, 200.0f / 255.0f, 1.0f, 1.0f);
constexpr ImVec4 kTrackColor = ImVec4(26.0f / 255.0f, 26.0f / 255.0f, 26.0f / 255.0f, 1.0f);

ImFont* g_body_font = nullptr;
ImFont* g_value_font = nullptr;
ImFont* g_label_font = nullptr;
ImFont* g_header_font = nullptr;

ImFont* fallback_font(ImFont* font) {
    return font != nullptr ? font : ImGui::GetFont();
}

ImU32 thermal_color(double norm) {
    norm = std::clamp(norm, 0.0, 1.0);
    const ImVec4 color = ImColor::HSV(static_cast<float>((1.0 - norm) * 0.68), 0.9f, 0.98f);
    return ImGui::ColorConvertFloat4ToU32(color);
}

ImVec4 utilization_color(float pct) {
    if (pct >= 0.8f) return ImVec4(1.0f, 0.35f, 0.28f, 1.0f);
    if (pct >= 0.6f) return ImVec4(1.0f, 0.73f, 0.28f, 1.0f);
    return ImVec4(0.10f, 0.88f, 0.50f, 1.0f);
}

const char* section_subtitle(AppSection section) {
    switch (section) {
        case AppSection::Process: return "Process lineage, IPC edges, and host activity context";
        case AppSection::Network: return "Connection map with remote endpoints and anomaly markers";
        case AppSection::Thermal: return "Die temperatures, cluster load, and accelerator pressure";
        case AppSection::Disk: return "Recent disk I/O events and high-latency access traces";
        case AppSection::Timeline: return "Replay sampled history from the rolling on-disk buffer";
        case AppSection::Settings: return "Runtime mode and helper link diagnostics";
        default: return "";
    }
}

void draw_section_header(const char* title, const char* subtitle) {
    ImGui::PushFont(fallback_font(g_header_font));
    ImGui::TextColored(kTextPrimary, "%s", title);
    ImGui::PopFont();

    ImGui::PushFont(fallback_font(g_label_font));
    ImGui::TextColored(kTextMuted, "%s", subtitle);
    ImGui::PopFont();

    const ImVec2 line_start = ImGui::GetCursorScreenPos();
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    ImGui::GetWindowDrawList()->AddLine(line_start, ImVec2(line_start.x + width, line_start.y),
                                        ImGui::GetColorU32(kBorderColor), 0.5f);
    ImGui::Dummy(ImVec2(width, 1.0f));
    ImGui::Dummy(ImVec2(0.0f, 12.0f));
}

void StatCard(const char* label, const char* value, const char* unit, ImVec4 valueColor) {
    constexpr float kCardWidth = 180.0f;
    constexpr float kCardHeight = 62.0f;
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImGui::BeginGroup();
    ImGui::PushFont(fallback_font(g_label_font));
    ImGui::TextColored(kTextMuted, "%s", label);
    ImGui::PopFont();

    ImGui::PushFont(fallback_font(g_value_font));
    ImGui::TextColored(valueColor, "%s", value);
    const float value_right = ImGui::GetItemRectMax().x;
    ImGui::PopFont();

    if (unit != nullptr && unit[0] != '\0') {
        ImGui::SameLine(0.0f, 4.0f);
        ImGui::PushFont(fallback_font(g_label_font));
        ImGui::TextColored(kTextMuted, "%s", unit);
        ImGui::PopFont();
    }

    const float consumed_h = ImGui::GetCursorScreenPos().y - start.y;
    if (consumed_h < kCardHeight - 8.0f) ImGui::Dummy(ImVec2(0.0f, (kCardHeight - 8.0f) - consumed_h));
    ImGui::Dummy(ImVec2(kCardWidth - std::max(0.0f, value_right - start.x), 0.0f));
    ImGui::EndGroup();

    draw_list->AddLine(ImVec2(start.x, start.y + kCardHeight), ImVec2(start.x + kCardWidth, start.y + kCardHeight),
                       ImGui::GetColorU32(kBorderColor), 0.5f);
}

void DrawBar(const char* label, float percent, bool use_heat_color) {
    percent = std::clamp(percent, 0.0f, 1.0f);
    ImGui::PushFont(fallback_font(g_label_font));
    ImGui::TextColored(kTextMuted, "%s", label);
    ImGui::PopFont();

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float available = std::max(100.0f, ImGui::GetContentRegionAvail().x);
    char pct_text[16];
    std::snprintf(pct_text, sizeof(pct_text), "%.0f%%", percent * 100.0f);
    const float pct_w = ImGui::CalcTextSize(pct_text).x;
    const float bar_w = std::max(10.0f, available - pct_w - 10.0f);
    constexpr float kRowHeight = 16.0f;
    constexpr float kBarHeight = 4.0f;
    const float bar_y = pos.y + (kRowHeight - kBarHeight) * 0.5f;
    const ImVec4 fill = use_heat_color ? utilization_color(percent) : kAccentColor;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(ImVec2(pos.x, bar_y), ImVec2(pos.x + bar_w, bar_y + kBarHeight),
                             ImGui::GetColorU32(kTrackColor), 2.0f);
    if (percent > 0.0f) {
        draw_list->AddRectFilled(ImVec2(pos.x, bar_y), ImVec2(pos.x + bar_w * percent, bar_y + kBarHeight),
                                 ImGui::GetColorU32(fill), 2.0f);
    }
    draw_list->AddText(ImVec2(pos.x + bar_w + 10.0f, pos.y), ImGui::GetColorU32(kTextMuted), pct_text);
    ImGui::Dummy(ImVec2(available, kRowHeight));
}

void draw_heatmap_grid(const std::vector<std::vector<double>>& grid) {
    const size_t rows = grid.size();
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
        ImGui::TextColored(kTextMuted, "Heatmap is waiting for temperature samples.");
        return;
    }

    ImGui::PushFont(fallback_font(g_label_font));
    ImGui::TextColored(kTextMuted, "Grid %zux%zu  min %.1f C  max %.1f C  avg %.1f C", rows, cols, min_t, max_t,
                       sum_t / static_cast<double>(value_count));
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    constexpr float kContainerPad = 8.0f;
    constexpr float kGap = 2.0f;
    const float avail_width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    float cell_size = (avail_width - (2.0f * kContainerPad) - (kGap * static_cast<float>(cols - 1))) /
                      static_cast<float>(cols);
    cell_size = std::max(8.0f, cell_size);

    const float grid_width = cell_size * static_cast<float>(cols) + kGap * static_cast<float>(cols - 1);
    const float grid_height = cell_size * static_cast<float>(rows) + kGap * static_cast<float>(rows - 1);
    const ImVec2 container_min = ImGui::GetCursorScreenPos();
    const ImVec2 grid_origin(container_min.x + kContainerPad, container_min.y + kContainerPad);
    const ImVec2 container_max(container_min.x + grid_width + 2.0f * kContainerPad,
                               container_min.y + grid_height + 2.0f * kContainerPad);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRect(container_min, container_max, ImGui::GetColorU32(kBorderColor), 0.0f, 0, 0.5f);

    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < grid[r].size(); ++c) {
            const double value = grid[r][c];
            const double norm = max_t > min_t ? (value - min_t) / (max_t - min_t) : 0.5;
            const float x = grid_origin.x + static_cast<float>(c) * (cell_size + kGap);
            const float y = grid_origin.y + static_cast<float>(r) * (cell_size + kGap);
            const ImVec2 cell_min(x, y);
            const ImVec2 cell_max(x + cell_size, y + cell_size);
            draw_list->AddRectFilled(cell_min, cell_max, thermal_color(norm), 2.0f);
            if (ImGui::IsMouseHoveringRect(cell_min, cell_max)) {
                ImGui::SetTooltip("Row %zu Col %zu \xE2\x80\x94 %.1f \xC2\xB0""C", r + 1, c + 1, value);
            }
        }
    }

    ImGui::Dummy(ImVec2(grid_width + 2.0f * kContainerPad, grid_height + 2.0f * kContainerPad));
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    constexpr float kLegendHeight = 10.0f;
    const ImVec2 legend_origin = ImGui::GetCursorScreenPos();
    constexpr int kSegments = 32;
    for (int i = 0; i < kSegments; ++i) {
        const float x0 = legend_origin.x + (grid_width * i) / kSegments;
        const float x1 = legend_origin.x + (grid_width * (i + 1)) / kSegments;
        draw_list->AddRectFilled(ImVec2(x0, legend_origin.y), ImVec2(x1, legend_origin.y + kLegendHeight),
                                 thermal_color(static_cast<double>(i) / (kSegments - 1)));
    }
    draw_list->AddRect(legend_origin, ImVec2(legend_origin.x + grid_width, legend_origin.y + kLegendHeight),
                       ImGui::GetColorU32(kBorderColor), 0.0f, 0, 0.5f);
    ImGui::Dummy(ImVec2(grid_width, kLegendHeight));

    ImGui::PushFont(fallback_font(g_label_font));
    ImGui::TextColored(kTextMuted, "Cold");
    const float hot_w = ImGui::CalcTextSize("Hot").x;
    ImGui::SameLine(std::max(0.0f, grid_width - hot_w));
    ImGui::TextColored(kTextMuted, "Hot");
    ImGui::PopFont();
}

}  // namespace

AppState::AppState() {
    const auto dir = std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : "/tmp") /
                     "Library/Application Support/Marrow";
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

void AppState::set_sidebar_logo(ImTextureID texture) {
    sidebar_logo_texture_ = texture;
}

void AppState::set_fonts(ImFont* body, ImFont* value, ImFont* label, ImFont* header) {
    body_font_ = body;
    value_font_ = value;
    label_font_ = label;
    header_font_ = header;
    g_body_font = body_font_;
    g_value_font = value_font_;
    g_label_font = label_font_;
    g_header_font = header_font_;
}

void AppState::draw_sparkline(const char* id, const std::deque<float>& data, float r, float g, float b) {
    if (data.empty()) return;
    std::vector<float> buf(data.begin(), data.end());
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(r, g, b, 1.f));
    ImGui::PlotLines(id, buf.data(), static_cast<int>(buf.size()), 0, nullptr, 0.f, 100.f, ImVec2(80, 28));
    ImGui::PopStyleColor();
}

void AppState::draw_sidebar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, kBgSidebar);
    ImGui::BeginChild("sidebar", ImVec2(208, 0), true);
    ImGui::PushFont(fallback_font(label_font_));
    if (sidebar_logo_texture_ != 0) {
        ImGui::Image(sidebar_logo_texture_, ImVec2(32.0f, 32.0f));
    } else {
        ImGui::TextColored(kAccentColor, "Marrow");
    }
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, 12.0f));

    for (int i = 0; i < static_cast<int>(AppSection::COUNT); ++i) {
        const auto sec = static_cast<AppSection>(i);
        const bool active = section == sec;
        const ImVec2 item_pos = ImGui::GetCursorScreenPos();
        const ImVec2 item_size(std::max(1.0f, ImGui::GetContentRegionAvail().x), 28.0f);
        char id[32];
        std::snprintf(id, sizeof(id), "##nav_%d", i);
        if (ImGui::InvisibleButton(id, item_size)) section = sec;

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        if (active) {
            draw_list->AddLine(ImVec2(item_pos.x, item_pos.y + 3.0f), ImVec2(item_pos.x, item_pos.y + item_size.y - 3.0f),
                               ImGui::GetColorU32(kAccentColor), 2.0f);
        }

        ImGui::PushFont(fallback_font(body_font_));
        draw_list->AddText(ImVec2(item_pos.x + 12.0f, item_pos.y + 6.0f),
                           ImGui::GetColorU32(active ? kTextPrimary : kTextMuted), section_name(sec));
        ImGui::PopFont();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::SameLine();
}

void AppState::draw_process() {
    if (!snapshot.has_process_graph) {
        ImGui::PushFont(fallback_font(label_font_));
        ImGui::TextColored(kTextMuted, "Waiting for process graph...");
        ImGui::PopFont();
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
    if (!snapshot.has_network) return;
    ImGui::PushFont(fallback_font(body_font_));
    ImGui::TextColored(kTextPrimary, "In: %.0f B/s  Out: %.0f B/s", snapshot.network.bytes_in_per_sec,
                       snapshot.network.bytes_out_per_sec);
    ImGui::PopFont();
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
            if (c.is_anomaly) ImGui::TextColored(ImVec4(1.0f, 0.73f, 0.28f, 1.0f), "ANOMALY");
        }
        ImGui::EndTable();
    }
}

void AppState::draw_thermal() {
    if (!snapshot.has_thermal) {
        ImGui::PushFont(fallback_font(label_font_));
        ImGui::TextColored(kTextMuted, "Waiting for thermal samples...");
        ImGui::PopFont();
        return;
    }

    const ImVec4 fan_color = snapshot.thermal.fan_rpm > 0 ? kTextPrimary : kTextMuted;
    StatCard("CPU die", std::to_string(static_cast<int>(snapshot.thermal.cpu_die_temp_celsius)).c_str(), "C",
             kTextPrimary);
    ImGui::SameLine(0.0f, 16.0f);
    StatCard("GPU die", std::to_string(static_cast<int>(snapshot.thermal.gpu_die_temp_celsius)).c_str(), "C",
             kTextPrimary);
    ImGui::SameLine(0.0f, 16.0f);
    StatCard("GPU util", std::to_string(static_cast<int>(snapshot.thermal.gpu_utilization_percent)).c_str(), "%",
             kTextPrimary);
    ImGui::SameLine(0.0f, 16.0f);
    if (snapshot.thermal.fan_rpm > 0) {
        StatCard("Fan", std::to_string(snapshot.thermal.fan_rpm).c_str(), "RPM", fan_color);
    } else {
        StatCard("Fan", "n/a", "", fan_color);
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    draw_heatmap_grid(snapshot.thermal.die_temperature_grid);
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    if (!snapshot.thermal.clusters.empty()) {
        ImGui::PushFont(fallback_font(label_font_));
        ImGui::TextColored(kTextMuted, "Clusters");
        ImGui::PopFont();
        for (size_t i = 0; i < snapshot.thermal.clusters.size(); ++i) {
            const auto& cl = snapshot.thermal.clusters[i];
            const std::string label = cl.name + " util";
            StatCard(label.c_str(), std::to_string(static_cast<int>(cl.utilization_percent)).c_str(), "%", kTextPrimary);
            if (i + 1 < snapshot.thermal.clusters.size()) ImGui::SameLine(0.0f, 16.0f);
        }
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        for (const auto& cl : snapshot.thermal.clusters) {
            DrawBar(cl.name.c_str(), static_cast<float>(cl.utilization_percent / 100.0), true);
        }
    }

    ImGui::PushFont(fallback_font(label_font_));
    ImGui::TextColored(kTextMuted, "Accelerators");
    ImGui::PopFont();
    StatCard("GPU", std::to_string(static_cast<int>(snapshot.thermal.gpu_utilization_percent)).c_str(), "%",
             kTextPrimary);
    ImGui::SameLine(0.0f, 16.0f);
    StatCard("ANE", std::to_string(static_cast<int>(snapshot.thermal.ane_utilization_percent)).c_str(), "%",
             kTextPrimary);
    DrawBar("GPU load", static_cast<float>(snapshot.thermal.gpu_utilization_percent / 100.0), true);
    DrawBar("ANE load", static_cast<float>(snapshot.thermal.ane_utilization_percent / 100.0), true);
}

void AppState::draw_disk() {
    if (!snapshot.has_disk) return;
    ImGui::PushFont(fallback_font(body_font_));
    ImGui::TextColored(kTextPrimary, "Read %.0f B/s  Write %.0f B/s", snapshot.disk.read_bytes_per_sec,
                       snapshot.disk.write_bytes_per_sec);
    ImGui::PopFont();
    for (const auto& e : snapshot.disk.recent_events) {
        ImGui::BulletText("%s  %s", e.process_name.c_str(), e.path.c_str());
        ImGui::SameLine();
        ImGui::TextColored(kTextMuted, "R:%llu W:%llu %.1fms", e.bytes_read, e.bytes_written, e.latency_ms);
    }
}

void AppState::draw_timeline() {
    ImGui::SliderFloat("Scrub", &timeline_scrub, 0.f, 1.f);
    DrawBar("Timeline position", timeline_scrub, false);
    if (!cpu_history.empty()) {
        std::vector<float> buf(cpu_history.begin(), cpu_history.end());
        ImGui::PlotLines("CPU history", buf.data(), static_cast<int>(buf.size()), 0, nullptr, 0.f, 100.f,
                         ImVec2(-1, 80));
    }
    ImGui::PushFont(fallback_font(label_font_));
    ImGui::TextColored(kTextMuted, "Ring buffer: %d bytes on disk", ring_buffer_->disk_usage_bytes());
    ImGui::PopFont();
}

void AppState::draw_settings() {
    ImGui::BeginDisabled();
    ImGui::Checkbox("Use stub providers (scaffold)", &use_stub_providers);
    ImGui::EndDisabled();
    ImGui::Text("Helper: %s", helper_connected ? "connected" : "offline");
    ImGui::Text("Version: %s", helper_version.c_str());
    ImGui::Separator();
    ImGui::PushFont(fallback_font(label_font_));
    ImGui::TextColored(kTextMuted, "Retention: 30 min  |  Disk cap: 200 MB");
    ImGui::PopFont();
}

void AppState::draw() {
    draw_sidebar();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(17.0f / 255.0f, 17.0f / 255.0f, 17.0f / 255.0f, 1.0f));
    ImGui::BeginChild("content", ImVec2(0, 0), true);
    draw_section_header(section_name(section), section_subtitle(section));
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
    ImGui::PopStyleColor();
}

}  // namespace marrow
