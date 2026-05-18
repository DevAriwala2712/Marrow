#pragma once

#include "sysscope/metric_provider.hpp"

#include <cstdint>
#include <unordered_map>

namespace sysscope {

class ProcessProvider : public IMetricProvider {
public:
    MetricKind kind() const override { return MetricKind::ProcessGraph; }
    void tick(MetricsSnapshot& snapshot) override;

private:
    struct ProcSample {
        std::uint64_t user_ns = 0;
        std::uint64_t sys_ns = 0;
        double at = 0;
    };
    std::unordered_map<std::int32_t, ProcSample> prev_;
    double last_wall_ = 0;
};

}  // namespace sysscope
