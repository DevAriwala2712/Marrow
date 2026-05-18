#pragma once

#include "sysscope/metric_provider.hpp"

#include <cstdint>
#include <string>

namespace sysscope {

class NetworkProvider : public IMetricProvider {
public:
    MetricKind kind() const override { return MetricKind::Network; }
    void tick(MetricsSnapshot& snapshot) override;

private:
    bool has_prev_ = false;
    double prev_at_ = 0;
    std::uint64_t prev_in_ = 0;
    std::uint64_t prev_out_ = 0;
    std::string iface_;
};

}  // namespace sysscope
