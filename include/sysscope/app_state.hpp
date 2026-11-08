#pragma once

#include "sysscope/metric_provider.hpp"
#include "sysscope/ring_buffer.hpp"

#include <array>
#include <deque>
#include <memory>

struct ImFont;

namespace sysscope {

enum class AppSection { Overview, Processes, Network, Disk, Thermal, History, Count };

class AppState {
public:
    AppState(bool demo, ImFont* heading_font, ImFont* value_font, unsigned int logo_texture);
    ~AppState();

    void tick(double dt);
    void draw();

private:
    void collect();
    void draw_sidebar();
    void draw_header(const char* title, const char* subtitle);
    void draw_overview();
    void draw_processes();
    void draw_process_table(bool compact);
    void draw_network();
    void draw_disk();
    void draw_thermal();
    void draw_history();
    void draw_cpu_chart(int seconds, float height);

    bool demo_;
    ImFont* heading_font_;
    ImFont* value_font_;
    unsigned int logo_texture_;
    AppSection section_ = AppSection::Overview;
    MetricAggregator aggregator_;
    MetricsSnapshot snapshot_;
    std::unique_ptr<IRingBufferStore> history_store_;
    std::deque<CpuSample> cpu_history_;
    bool history_saved_ = false;
    double sample_elapsed_ = 0;
    std::array<char, 128> process_query_{};
    int history_window_ = 60;
};

}  // namespace sysscope
