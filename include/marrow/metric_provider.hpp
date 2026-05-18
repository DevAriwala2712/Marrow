#pragma once

#include "marrow/types.hpp"
#include "marrow/util.hpp"

#include <memory>
#include <vector>

namespace marrow {

class IMetricProvider {
public:
    virtual ~IMetricProvider() = default;
    virtual MetricKind kind() const = 0;
    virtual void start() {}
    virtual void stop() {}
    virtual void tick(MetricsSnapshot& snapshot) = 0;
};

using MetricProviderPtr = std::shared_ptr<IMetricProvider>;

class MetricAggregator {
public:
    void set_providers(std::vector<MetricProviderPtr> providers) {
        providers_ = std::move(providers);
        for (auto& p : providers_) p->start();
    }

    void stop() {
        for (auto& p : providers_) p->stop();
    }

    MetricsSnapshot collect() {
        MetricsSnapshot snap;
        snap.timestamp = now_seconds();
        for (auto& p : providers_) p->tick(snap);
        return snap;
    }

private:
    std::vector<MetricProviderPtr> providers_;
};

}  // namespace marrow
