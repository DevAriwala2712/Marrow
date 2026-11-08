#include "sysscope/app_state.hpp"

#include "imgui.h"
#include "sysscope/provider_factory.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <strings.h>

namespace sysscope {
namespace {

constexpr ImVec4 teal{0.35f, 0.90f, 0.79f, 1};
constexpr ImVec4 blue{0.52f, 0.72f, 1.00f, 1};
constexpr ImVec4 violet{0.78f, 0.66f, 0.99f, 1};
constexpr ImVec4 amber{1.00f, 0.75f, 0.46f, 1};
constexpr ImU32 muted = IM_COL32(151, 165, 185, 255);
constexpr double retention = 30 * 60;
constexpr const char* sections[] = {"Overview", "Processes", "Network", "Disk", "Thermal", "History"};

std::string percent(double value) {
    char text[24];
    std::snprintf(text, sizeof(text), "%.1f%%", value);
    return text;
}

std::string bytes(double value, bool rate = false) {
    const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    size_t i = 0;
    while (value >= 1024 && i < 4) {
        value /= 1024;
        ++i;
    }
    char text[32];
    std::snprintf(text, sizeof(text), i == 0 ? "%.0f %s%s" : "%.1f %s%s",
                  value, units[i], rate ? "/s" : "");
    return text;
}

void caption(const char* text, ImVec4 color = teal) {
    ImGui::TextColored(color, "%s", text);
}

void metric_card(const char* label, const std::string& value, const std::string& detail,
                 ImVec4 color, ImFont* font) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {13, 10});
    ImGui::BeginChild(label, {0, 105}, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    caption(label, color);
    ImGui::PushFont(font);
    ImGui::TextUnformatted(value.c_str());
    ImGui::PopFont();
    ImGui::TextDisabled("%s", detail.c_str());
    ImGui::EndChild();
}

void heading(const char* title, const char* detail) {
    ImGui::TextUnformatted(title);
    ImGui::TextDisabled("%s", detail);
}

ImU32 heat_color(double position) {
    const auto t = static_cast<float>(std::clamp(position, 0.0, 1.0));
    return ImGui::ColorConvertFloat4ToU32(
        {0.16f + 0.82f * t, 0.48f + 0.22f * t, 0.63f - 0.29f * t, 1.0f});
}

}  // namespace

AppState::AppState(bool demo, ImFont* heading_font, ImFont* value_font, unsigned int logo_texture)
    : demo_(demo), heading_font_(heading_font), value_font_(value_font), logo_texture_(logo_texture) {
    aggregator_.set_providers(demo ? make_stub_providers() : make_real_providers());
    if (demo_) {
        const double now = now_seconds();
        for (int i = 90; i > 0; --i) {
            const auto sample = aggregator_.collect();
            cpu_history_.push_back({now - i, static_cast<float>(sample.cpu.total_used())});
        }
    } else if (const auto* home = std::getenv("HOME"); home && *home) {
        // Keep the existing location so earlier installations can open the same database.
        const auto dir = std::filesystem::path(home) / "Library/Application Support/SysScope";
        std::error_code error;
        std::filesystem::create_directories(dir, error);
        if (!error) {
            history_store_ = make_sqlite_ring_buffer();
            if (history_store_->open((dir / "history.sqlite").string())) {
                history_saved_ = true;
                const auto saved = history_store_->cpu_history(now_seconds() - retention);
                cpu_history_.assign(saved.begin(), saved.end());
            }
        }
    }
    collect();
}

AppState::~AppState() {
    aggregator_.stop();
}

void AppState::collect() {
    snapshot_ = aggregator_.collect();
    if (snapshot_.has_cpu) {
        cpu_history_.push_back({snapshot_.timestamp, static_cast<float>(snapshot_.cpu.total_used())});
        if (history_store_) history_saved_ = history_store_->append(snapshot_);
    }
    while (!cpu_history_.empty() && cpu_history_.front().timestamp < snapshot_.timestamp - retention)
        cpu_history_.pop_front();
}

void AppState::tick(double dt) {
    sample_elapsed_ += dt;
    if (sample_elapsed_ < 1) return;
    sample_elapsed_ = 0;
    collect();
}

void AppState::draw_sidebar() {
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {16, 20});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.073f, 0.093f, 0.126f, 1});
    ImGui::BeginChild("navigation", {182, 0}, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    if (logo_texture_) {
        ImGui::Image(static_cast<ImTextureID>(logo_texture_), {34, 34});
        ImGui::SameLine(0, 9);
    }
    ImGui::PushFont(value_font_);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
    ImGui::TextUnformatted("Marrow");
    ImGui::PopFont();

    ImGui::Dummy({0, 16});
    ImGui::TextDisabled("  MONITOR");
    for (int i = 0; i < static_cast<int>(AppSection::Count); ++i) {
        const auto item = static_cast<AppSection>(i);
        const bool selected = section_ == item;
        const std::string label = std::string("  ") + sections[i];
        if (selected) ImGui::PushStyleColor(ImGuiCol_Text, teal);
        if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_None, {0, 25})) {
            section_ = item;
        }
        if (selected) ImGui::PopStyleColor();
    }

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 70);
    ImGui::Separator();
    ImGui::TextDisabled("  macOS");
    ImGui::TextDisabled("  v%s", MARROW_VERSION);
    ImGui::EndChild();
}

void AppState::draw_header(const char* title, const char* subtitle) {
    const char* mode = demo_ ? "DEMO DATA" : "LIVE  /  1 SEC";
    const auto pos = ImGui::GetCursorScreenPos();
    const float right = pos.x + ImGui::GetContentRegionAvail().x;
    const auto tint = ImGui::GetColorU32(demo_ ? amber : teal);
    auto* draw = ImGui::GetWindowDrawList();
    const auto width = ImGui::CalcTextSize(mode).x;
    draw->AddRectFilled({right - width - 35, pos.y + 4}, {right, pos.y + 32},
                        demo_ ? IM_COL32(58, 43, 32, 255) : IM_COL32(22, 52, 54, 255), 7);
    draw->AddCircleFilled({right - width - 23, pos.y + 18}, 3, tint);
    draw->AddText({right - width - 12, pos.y + 9}, tint, mode);
    ImGui::PushFont(heading_font_);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    ImGui::TextDisabled("%s", subtitle);
    ImGui::Dummy({0, 3});
}

void AppState::draw_overview() {
    draw_header("Overview", demo_ ? "A preview of system activity" : "A live view of system activity");
    if (ImGui::BeginTable("overview-cards", 4, ImGuiTableFlags_SizingStretchSame |
                                              ImGuiTableFlags_NoPadOuterX)) {
        ImGui::TableNextColumn();
        metric_card("CPU LOAD", snapshot_.has_cpu ? percent(snapshot_.cpu.total_used()) : "--",
                    "All logical cores", teal, value_font_);
        ImGui::TableNextColumn();
        metric_card("MEMORY", snapshot_.has_memory ? bytes(snapshot_.memory.used_bytes) : "--",
                    snapshot_.has_memory ? "of " + bytes(snapshot_.memory.total_bytes) : "Physical memory",
                    violet, value_font_);
        ImGui::TableNextColumn();
        metric_card("NETWORK IN", snapshot_.has_network ? bytes(snapshot_.network.bytes_in_per_sec, true) : "--",
                    "Active interfaces", blue, value_font_);
        ImGui::TableNextColumn();
        metric_card("DISK WRITE", snapshot_.has_disk ? bytes(snapshot_.disk.write_bytes_per_sec, true) : "--",
                    "Storage devices", amber, value_font_);
        ImGui::EndTable();
    }

    ImGui::Dummy({0, 2});
    heading("CPU activity", "Total utilization over the last 60 seconds");
    draw_cpu_chart(60, 194);
    ImGui::Dummy({0, 2});
    heading("Top processes", "Ranked by share of total CPU capacity");
    draw_process_table(true);
}

void AppState::draw_process_table(bool compact) {
    if (!snapshot_.has_process_graph) {
        ImGui::TextDisabled("Process data is currently unavailable.");
        return;
    }
    const auto flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                       ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchProp;
    if (!ImGui::BeginTable("processes", compact ? 4 : 6, flags)) return;
    ImGui::TableSetupColumn("Process", ImGuiTableColumnFlags_WidthStretch, 2.1f);
    ImGui::TableSetupColumn("PID");
    ImGui::TableSetupColumn("CPU");
    ImGui::TableSetupColumn("Memory");
    if (!compact) {
        ImGui::TableSetupColumn("Parent PID");
        ImGui::TableSetupColumn("State");
    }
    ImGui::TableHeadersRow();
    size_t shown = 0;
    for (const auto& process : snapshot_.process_graph.nodes) {
        if (!compact && process_query_[0] &&
            !strcasestr(process.name.c_str(), process_query_.data()) &&
            !strcasestr(std::to_string(process.id).c_str(), process_query_.data())) continue;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(process.name.c_str());
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%d", process.id);
        ImGui::TableNextColumn();
        ImGui::TextColored(teal, "%s", percent(process.cpu_percent).c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%s", bytes(process.mem_bytes).c_str());
        if (!compact) {
            ImGui::TableNextColumn();
            ImGui::Text("%d", process.parent_pid);
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", process.status.c_str());
        }
        if (++shown == 4 && compact) break;
    }
    ImGui::EndTable();
    if (shown == 0) ImGui::TextDisabled("No matching processes.");
}

void AppState::draw_processes() {
    draw_header("Processes", "CPU, resident memory, and parent IDs for visible processes");
    ImGui::SetNextItemWidth(std::min(380.0f, ImGui::GetContentRegionAvail().x));
    ImGui::InputTextWithHint("##process-search", "Filter by process name or PID", process_query_.data(),
                             process_query_.size());
    draw_process_table(false);
}

void AppState::draw_network() {
    draw_header("Network", "Combined traffic across active non-loopback interfaces");
    if (!snapshot_.has_network) {
        ImGui::TextDisabled("Network counters are currently unavailable.");
        return;
    }
    if (ImGui::BeginTable("network-cards", 2, ImGuiTableFlags_SizingStretchSame |
                                             ImGuiTableFlags_NoPadOuterX)) {
        ImGui::TableNextColumn();
        metric_card("RECEIVED", bytes(snapshot_.network.bytes_in_per_sec, true), "Download rate", blue, value_font_);
        ImGui::TableNextColumn();
        metric_card("SENT", bytes(snapshot_.network.bytes_out_per_sec, true), "Upload rate", teal, value_font_);
        ImGui::EndTable();
    }
}

void AppState::draw_disk() {
    draw_header("Disk", "Aggregate throughput reported by macOS block storage drivers");
    if (!snapshot_.has_disk) {
        ImGui::TextDisabled("Disk throughput counters are unavailable on this Mac.");
        return;
    }
    if (ImGui::BeginTable("disk-cards", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
        ImGui::TableNextColumn();
        metric_card("READ", bytes(snapshot_.disk.read_bytes_per_sec, true), "Across reporting devices", blue, value_font_);
        ImGui::TableNextColumn();
        metric_card("WRITE", bytes(snapshot_.disk.write_bytes_per_sec, true), "Across reporting devices", amber, value_font_);
        ImGui::EndTable();
    }
}

void AppState::draw_thermal() {
    draw_header("Thermal", demo_ ? "A simulated die map and sample accelerator readings"
                                 : "System thermal pressure reported by macOS");
    if (!snapshot_.has_thermal) {
        ImGui::TextDisabled("Thermal pressure is currently unavailable.");
        return;
    }

    const auto& thermal = snapshot_.thermal;
    if (!demo_) {
        const auto tint = thermal.pressure == "Nominal" ? teal : amber;
        metric_card("THERMAL PRESSURE", thermal.pressure, "Current system state", tint, value_font_);
        ImGui::TextDisabled("Thermal pressure describes the system's response to heat.");
        ImGui::TextDisabled("States  /  Nominal, Fair, Serious, Critical");
        return;
    }

    if (ImGui::BeginTable("thermal-cards", 3, ImGuiTableFlags_SizingStretchSame |
                                             ImGuiTableFlags_NoPadOuterX)) {
        ImGui::TableNextColumn();
        metric_card("GPU", percent(thermal.gpu_utilization_percent), "Sample utilization", violet, value_font_);
        ImGui::TableNextColumn();
        metric_card("NEURAL ENGINE", percent(thermal.ane_utilization_percent), "Sample utilization", blue, value_font_);
        ImGui::TableNextColumn();
        metric_card("SENSOR GRID", "8 x 8", "Simulated readings", amber, value_font_);
        ImGui::EndTable();
    }

    const auto& grid = thermal.die_temperature_grid;
    if (grid.empty() || grid.front().empty()) return;
    ImGui::Dummy({0, 4});
    heading("Simulated die map", "Hover a tile to inspect its sample temperature");
    const float cell = std::min(38.0f, ImGui::GetContentRegionAvail().x / grid.front().size());
    const auto origin = ImGui::GetCursorScreenPos();
    auto* draw = ImGui::GetWindowDrawList();
    for (size_t r = 0; r < grid.size(); ++r) {
        for (size_t c = 0; c < grid[r].size(); ++c) {
            const ImVec2 tile{origin.x + c * cell, origin.y + r * cell};
            const ImVec2 corner{tile.x + cell - 3, tile.y + cell - 3};
            draw->AddRectFilled(tile, corner, heat_color((grid[r][c] - 26) / 28), 5);
            if (ImGui::IsMouseHoveringRect(tile, corner)) {
                draw->AddRect(tile, corner, ImGui::GetColorU32(ImGuiCol_Text), 5);
                ImGui::SetTooltip("Row %zu, column %zu  /  %.1f C", r + 1, c + 1, grid[r][c]);
            }
        }
    }
    ImGui::Dummy({cell * grid.front().size(), cell * grid.size()});
    ImGui::TextDisabled("26 C  /  cooler");
    ImGui::SameLine(0, 30);
    ImGui::TextColored(amber, "54 C  /  warmer");
}

void AppState::draw_cpu_chart(int seconds, float height) {
    const auto origin = ImGui::GetCursorScreenPos();
    const ImVec2 size{ImGui::GetContentRegionAvail().x, height};
    ImGui::InvisibleButton("##cpu-chart", size);
    const bool hovered = ImGui::IsItemHovered();
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 corner{origin.x + size.x, origin.y + height};
    draw->AddRectFilled(origin, corner, IM_COL32(18, 24, 33, 255), 10);
    draw->AddRect(origin, corner, ImGui::GetColorU32(ImGuiCol_Border), 10);

    const ImVec2 start{origin.x + 51, origin.y + 16};
    const ImVec2 end{corner.x - 17, corner.y - 31};
    for (int i = 0; i <= 4; ++i) {
        const float y = start.y + (end.y - start.y) * i / 4;
        draw->AddLine({start.x, y}, {end.x, y}, IM_COL32(48, 60, 76, 255));
        const auto label = std::to_string(100 - i * 25) + "%";
        draw->AddText({origin.x + 9, y - 7}, muted, label.c_str());
    }
    const auto range_label = std::to_string(seconds / 60) + "m ago";
    draw->AddText({start.x, end.y + 8}, muted, range_label.c_str());
    draw->AddText({end.x - 28, end.y + 8}, muted, "Now");

    const double cutoff = snapshot_.timestamp - seconds;
    const auto point = [&](const CpuSample& sample) {
        const auto x = static_cast<float>(std::clamp((sample.timestamp - cutoff) / seconds, 0.0, 1.0));
        const float y = std::clamp(sample.percent, 0.0f, 100.0f) / 100;
        return ImVec2(start.x + (end.x - start.x) * x, end.y - (end.y - start.y) * y);
    };
    const CpuSample* previous = nullptr;
    const CpuSample* nearest = nullptr;
    float closest = 14.0f;
    for (const auto& sample : cpu_history_) {
        if (sample.timestamp < cutoff || sample.timestamp > snapshot_.timestamp) continue;
        const auto pos = point(sample);
        if (previous && sample.timestamp > previous->timestamp && sample.timestamp - previous->timestamp < 3) {
            const auto from = point(*previous);
            draw->AddQuadFilled(from, pos, {pos.x, end.y}, {from.x, end.y}, IM_COL32(70, 210, 181, 39));
            draw->AddLine(from, pos, ImGui::GetColorU32(teal), 1.8f);
        } else {
            draw->AddCircleFilled(pos, 2, ImGui::GetColorU32(teal));
        }
        previous = &sample;
        const float distance = std::abs(ImGui::GetIO().MousePos.x - pos.x);
        if (distance < closest) {
            closest = distance;
            nearest = &sample;
        }
    }
    if (hovered && nearest) {
        const auto pos = point(*nearest);
        draw->AddLine({pos.x, start.y}, {pos.x, end.y}, IM_COL32(124, 172, 178, 180));
        draw->AddCircleFilled(pos, 4, ImGui::GetColorU32(teal));
        ImGui::SetTooltip("%.1f%% CPU  /  %.0fs ago", nearest->percent, snapshot_.timestamp - nearest->timestamp);
    }
    if (!previous) draw->AddText({start.x + 12, start.y + 12}, muted, "Waiting for CPU samples...");
}

void AppState::draw_history() {
    draw_header("History", "CPU utilization over the last 30 minutes");
    constexpr int windows[] = {60, 300, 1800};
    ImGui::SetNextItemWidth(165);
    if (ImGui::BeginCombo("##history-window", (std::to_string(history_window_ / 60) + " min").c_str())) {
        for (int seconds : windows) {
            const auto label = std::to_string(seconds / 60) + " min";
            if (ImGui::Selectable(label.c_str(), history_window_ == seconds)) history_window_ = seconds;
        }
        ImGui::EndCombo();
    }
    draw_cpu_chart(history_window_, 360);
    ImGui::TextDisabled("%s", demo_ ? "Sample history stays in this session."
                                    : history_saved_ ? "CPU samples are saved locally."
                                                     : "Local storage is unavailable. New samples are kept in this session.");
}

void AppState::draw() {
    draw_sidebar();
    ImGui::SameLine(0, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {26, 22});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
    ImGui::BeginChild("content", {0, 0}, ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    switch (section_) {
        case AppSection::Overview: draw_overview(); break;
        case AppSection::Processes: draw_processes(); break;
        case AppSection::Network: draw_network(); break;
        case AppSection::Disk: draw_disk(); break;
        case AppSection::Thermal: draw_thermal(); break;
        case AppSection::History: draw_history(); break;
        case AppSection::Count: break;
    }
    ImGui::EndChild();
}

}  // namespace sysscope
