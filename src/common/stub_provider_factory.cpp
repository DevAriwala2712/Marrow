#include "sysscope/provider_factory.hpp"
#include "sysscope/stub_providers.hpp"

namespace sysscope {

std::vector<MetricProviderPtr> make_stub_providers() {
    return {
        std::make_shared<StubCpuProvider>(),
        std::make_shared<StubMemoryProvider>(),
        std::make_shared<StubNetworkProvider>(),
        std::make_shared<StubDiskProvider>(),
        std::make_shared<StubThermalProvider>(),
        std::make_shared<StubProcessGraphProvider>(),
    };
}

}  // namespace sysscope
