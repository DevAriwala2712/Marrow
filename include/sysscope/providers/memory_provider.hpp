#pragma once

#include "sysscope/metric_provider.hpp"

namespace sysscope {

class MemoryProvider : public IMetricProvider {
public:
    MetricKind kind() const override { return MetricKind::Memory; }
    void tick(MetricsSnapshot& snapshot) override;
};

}  // namespace sysscope
