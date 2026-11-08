#pragma once

#include "sysscope/metric_provider.hpp"

namespace sysscope {

class ThermalProvider : public IMetricProvider {
public:
    MetricKind kind() const override { return MetricKind::Thermal; }
    void tick(MetricsSnapshot& snapshot) override;
};

}  // namespace sysscope
