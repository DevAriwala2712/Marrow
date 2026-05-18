#pragma once

#include "sysscope/metric_provider.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace sysscope {

class ThermalProvider : public IMetricProvider {
public:
    MetricKind kind() const override { return MetricKind::Thermal; }
    void start() override;
    void stop() override;
    void tick(MetricsSnapshot& snapshot) override;

private:
    void reader_loop();
    void apply_json_line(const std::string& line);

    std::thread reader_;
    std::atomic<bool> running_{false};
    std::mutex mutex_;
    ThermalMetrics latest_;
    bool has_data_ = false;
};

}  // namespace sysscope
