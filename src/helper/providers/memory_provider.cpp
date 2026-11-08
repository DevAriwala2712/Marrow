#include "sysscope/providers/memory_provider.hpp"

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <sys/sysctl.h>

namespace sysscope {

void MemoryProvider::tick(MetricsSnapshot& snapshot) {
    mach_port_t host = mach_host_self();
    vm_size_t page_size = 0;
    const auto page_result = host_page_size(host, &page_size);
    vm_statistics64_data_t vm{};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    const auto stats_result = host_statistics64(host, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm), &count);
    mach_port_deallocate(mach_task_self(), host);
    if (page_result != KERN_SUCCESS || stats_result != KERN_SUCCESS) return;

    const auto pages = [&](std::uint64_t n) { return n * static_cast<std::uint64_t>(page_size); };

    const std::uint64_t active = pages(vm.active_count);
    const std::uint64_t wired = pages(vm.wire_count);
    const std::uint64_t compressed = pages(vm.compressor_page_count);
    const std::uint64_t speculative = pages(vm.speculative_count);
    const std::uint64_t external = pages(vm.external_page_count);

    std::uint64_t total = 0;
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    size_t len = sizeof(total);
    if (sysctl(mib, 2, &total, &len, nullptr, 0) != 0 || total == 0) return;

    const std::uint64_t used = active + wired + compressed;
    const std::uint64_t free = pages(vm.free_count);
    const std::uint64_t cached = speculative + external;

    snapshot.memory.used_bytes = used;
    snapshot.memory.total_bytes = total;
    snapshot.memory.wired_bytes = wired;
    snapshot.memory.compressed_bytes = compressed;
    snapshot.memory.cached_bytes = cached;
    snapshot.memory.free_bytes = free;
    snapshot.has_memory = true;
}

}  // namespace sysscope
