#pragma once

#include "sysscope/metric_provider.hpp"

namespace sysscope {

class CpuProvider : public IMetricProvider {
public:
    MetricKind kind() const override { return MetricKind::Cpu; }
    void start() override;
    void tick(MetricsSnapshot& snapshot) override;

private:
    struct Ticks {
        unsigned int user, nice, system, idle;
    };
    std::vector<Ticks> previous_;
};

}  // namespace sysscope
