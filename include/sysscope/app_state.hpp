#pragma once

#include "sysscope/ipc.hpp"
#include "sysscope/metric_provider.hpp"
#include "sysscope/ring_buffer.hpp"
#include "sysscope/stub_providers.hpp"
#include "sysscope/types.hpp"

#include <deque>
#include <memory>
#include <string>

namespace sysscope {

enum class AppSection {
    Process,
    Network,
    Thermal,
    Disk,
    Timeline,
    Settings,
    COUNT
};

inline const char* section_name(AppSection s) {
    switch (s) {
        case AppSection::Process: return "Process";
        case AppSection::Network: return "Network";
        case AppSection::Thermal: return "Thermal";
        case AppSection::Disk: return "Disk";
        case AppSection::Timeline: return "Timeline";
        case AppSection::Settings: return "Settings";
        default: return "?";
    }
}

class AppState {
public:
    AppState();

    void start();
    void tick(double dt);
    void draw();

    MetricsSnapshot snapshot;
    AppSection section = AppSection::Process;
    bool use_stub_providers = true;
    bool helper_connected = false;
    std::string helper_version = "-";
    float timeline_scrub = 1.0f;
    std::deque<float> cpu_history;

private:
    void draw_sidebar();
    void draw_process();
    void draw_network();
    void draw_thermal();
    void draw_disk();
    void draw_timeline();
    void draw_settings();
    void draw_sparkline(const char* id, const std::deque<float>& data, float r, float g, float b);

    MetricAggregator aggregator_;
    HelperClient helper_;
    std::unique_ptr<IRingBufferStore> ring_buffer_;
    double sample_accum_ = 0;
};

}  // namespace sysscope
