#pragma once

#include "sysscope/metric_provider.hpp"

namespace sysscope {

class CpuProvider : public IMetricProvider {
public:
    MetricKind kind() const override { return MetricKind::Cpu; }
    void start() override;
    void tick(MetricsSnapshot& snapshot) override;

private:
    bool initialized_ = false;
    int cpu_count_ = 0;
    std::vector<unsigned int> prev_ticks_;  // 4 ticks per cpu: user,nice,sys,idle
    double last_sample_time_ = 0;
};

}  // namespace sysscope
