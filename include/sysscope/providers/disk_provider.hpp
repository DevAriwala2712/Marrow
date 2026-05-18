#pragma once

#include "sysscope/metric_provider.hpp"

#include <cstdint>

namespace sysscope {

class DiskProvider : public IMetricProvider {
public:
    MetricKind kind() const override { return MetricKind::Disk; }
    void tick(MetricsSnapshot& snapshot) override;

private:
    bool has_prev_ = false;
    double prev_at_ = 0;
    std::uint64_t prev_read_ = 0;
    std::uint64_t prev_write_ = 0;
};

}  // namespace sysscope
