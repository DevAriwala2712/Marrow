#pragma once

#include "marrow/metric_provider.hpp"

#include <vector>

namespace marrow {

/// Stub providers (no privileges). Used by tests, CLI fallback, and app offline mode.
std::vector<MetricProviderPtr> make_stub_providers();

/// Real collectors for marrow-helper (sysctl/mach/libproc/IOKit/powermetrics).
std::vector<MetricProviderPtr> make_real_providers();

}  // namespace marrow
