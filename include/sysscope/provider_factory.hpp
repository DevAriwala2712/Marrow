#pragma once

#include "sysscope/metric_provider.hpp"

#include <vector>

namespace sysscope {

/// Stub providers (no privileges). Used by tests, CLI fallback, and app offline mode.
std::vector<MetricProviderPtr> make_stub_providers();

/// Real collectors for sysscope-helper (sysctl/mach/libproc/IOKit/powermetrics).
std::vector<MetricProviderPtr> make_real_providers();

}  // namespace sysscope
