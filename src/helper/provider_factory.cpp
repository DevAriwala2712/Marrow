#include "marrow/provider_factory.hpp"
#include "marrow/providers/cpu_provider.hpp"
#include "marrow/providers/disk_provider.hpp"
#include "marrow/providers/memory_provider.hpp"
#include "marrow/providers/network_provider.hpp"
#include "marrow/providers/process_provider.hpp"
#include "marrow/providers/thermal_provider.hpp"

namespace marrow {

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

}  // namespace marrow
