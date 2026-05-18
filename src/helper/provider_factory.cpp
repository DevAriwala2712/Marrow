#include "sysscope/provider_factory.hpp"
#include "sysscope/providers/cpu_provider.hpp"
#include "sysscope/providers/disk_provider.hpp"
#include "sysscope/providers/memory_provider.hpp"
#include "sysscope/providers/network_provider.hpp"
#include "sysscope/providers/process_provider.hpp"
#include "sysscope/providers/thermal_provider.hpp"

namespace sysscope {

std::vector<MetricProviderPtr> make_real_providers() {
    return {
        std::make_shared<CpuProvider>(),
        std::make_shared<MemoryProvider>(),
        std::make_shared<NetworkProvider>(),
        std::make_shared<DiskProvider>(),
        std::make_shared<ThermalProvider>(),
        std::make_shared<ProcessProvider>(),
    };
}

}  // namespace sysscope
