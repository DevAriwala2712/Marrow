#include "sysscope/providers/cpu_provider.hpp"

#include <mach/mach.h>
#include <mach/processor_info.h>

namespace sysscope {

void CpuProvider::start() {
    previous_.clear();
}

void CpuProvider::tick(MetricsSnapshot& snapshot) {
    processor_info_array_t info = nullptr;
    mach_msg_type_number_t count = 0;
    natural_t cores = 0;
    const auto host = mach_host_self();
    const auto result = host_processor_info(host, PROCESSOR_CPU_LOAD_INFO, &cores, &info, &count);
    mach_port_deallocate(mach_task_self(), host);
    if (result != KERN_SUCCESS || !info) return;

    const auto* loads = reinterpret_cast<processor_cpu_load_info_t>(info);
    std::vector<Ticks> current(cores);
    for (natural_t i = 0; i < cores; ++i) {
        current[i] = {loads[i].cpu_ticks[CPU_STATE_USER], loads[i].cpu_ticks[CPU_STATE_NICE],
                      loads[i].cpu_ticks[CPU_STATE_SYSTEM], loads[i].cpu_ticks[CPU_STATE_IDLE]};
    }
    vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(info), count * sizeof(integer_t));

    if (!current.empty() && previous_.size() == current.size()) {
        double user = 0;
        double system = 0;
        double idle = 0;
        for (size_t i = 0; i < current.size(); ++i) {
            const auto& cur = current[i];
            const auto& prev = previous_[i];
            const double core_user = static_cast<double>(cur.user - prev.user) + (cur.nice - prev.nice);
            const double core_system = cur.system - prev.system;
            const double core_idle = cur.idle - prev.idle;
            const double total = core_user + core_system + core_idle;
            snapshot.cpu.per_core_percent.push_back(total > 0 ? 100 * (core_user + core_system) / total : 0);
            user += core_user;
            system += core_system;
            idle += core_idle;
        }

        const double total = user + system + idle;
        if (total > 0) {
            snapshot.cpu.user_percent = 100 * user / total;
            snapshot.cpu.system_percent = 100 * system / total;
            snapshot.cpu.idle_percent = 100 * idle / total;
            snapshot.has_cpu = true;
        }
    }
    previous_ = std::move(current);
}

}  // namespace sysscope
