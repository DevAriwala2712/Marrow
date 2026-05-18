#pragma once

#include "marrow/metric_provider.hpp"

namespace marrow {

class MemoryProvider : public IMetricProvider {
public:
    MetricKind kind() const override { return MetricKind::Memory; }
    void tick(MetricsSnapshot& snapshot) override;
};

}  // namespace marrow
