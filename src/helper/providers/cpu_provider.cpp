#include "marrow/providers/cpu_provider.hpp"
#include "marrow/util.hpp"

#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/processor_info.h>
#include <mach/vm_map.h>
#include <sys/sysctl.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace marrow {

namespace {

bool sysctl_cpuload(std::vector<std::vector<unsigned int>>& out_ticks) {
    int ncpu = 0;
    size_t sz = sizeof(ncpu);
    if (sysctlbyname("hw.ncpu", &ncpu, &sz, nullptr, 0) != 0 || ncpu <= 0) return false;

    size_t load_sz = 0;
    if (sysctlbyname("kern.cpuload", nullptr, &load_sz, nullptr, 0) != 0 || load_sz == 0)
        return false;

    std::vector<std::byte> buf(load_sz);
    if (sysctlbyname("kern.cpuload", buf.data(), &load_sz, nullptr, 0) != 0) return false;

    // cpuload: array of cpu ticks [CPU_STATE_MAX] per logical CPU
    constexpr int kStates = 4;  // user, nice, sys, idle on Darwin
    const size_t per_cpu = kStates * sizeof(unsigned int);
    const size_t cpus = load_sz / per_cpu;
    if (cpus == 0) return false;

    out_ticks.resize(cpus);
    for (size_t i = 0; i < cpus; ++i) {
        out_ticks[i].resize(kStates);
        std::memcpy(out_ticks[i].data(), buf.data() + i * per_cpu, per_cpu);
    }
    (void)ncpu;
    return true;
}

bool host_processor_ticks(std::vector<std::vector<unsigned int>>& out_ticks, int& cpu_count) {
    processor_info_array_t info = nullptr;
    mach_msg_type_number_t count = 0;
    natural_t ncpus = 0;
    const kern_return_t kr = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &ncpus,
                                                 &info, &count);
    if (kr != KERN_SUCCESS || !info || ncpus == 0) return false;

    auto* load = reinterpret_cast<processor_cpu_load_info_t>(info);
    cpu_count = static_cast<int>(ncpus);
    out_ticks.resize(ncpus);
    for (natural_t i = 0; i < ncpus; ++i) {
        out_ticks[i] = {load[i].cpu_ticks[CPU_STATE_USER], load[i].cpu_ticks[CPU_STATE_NICE],
                        load[i].cpu_ticks[CPU_STATE_SYSTEM], load[i].cpu_ticks[CPU_STATE_IDLE]};
    }
    vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(info),
                  static_cast<vm_size_t>(count * sizeof(int)));
    return true;
}

double pct_from_ticks(const std::vector<unsigned int>& cur, const std::vector<unsigned int>& prev) {
    if (cur.size() < 4 || prev.size() < 4) return 0;
    unsigned long long duser = cur[0] - prev[0];
    unsigned long long dnice = cur[1] - prev[1];
    unsigned long long dsys = cur[2] - prev[2];
    unsigned long long didle = cur[3] - prev[3];
    const unsigned long long total = duser + dnice + dsys + didle;
    if (total == 0) return 0;
    const unsigned long long active = duser + dnice + dsys;
    return 100.0 * static_cast<double>(active) / static_cast<double>(total);
}

}  // namespace

void CpuProvider::start() {
    initialized_ = false;
    prev_ticks_.clear();
}

void CpuProvider::tick(MetricsSnapshot& snapshot) {
    std::vector<std::vector<unsigned int>> cur;
    int n = 0;
    if (!host_processor_ticks(cur, n)) {
        if (!sysctl_cpuload(cur)) return;
        n = static_cast<int>(cur.size());
    }
    cpu_count_ = n;

    if (!initialized_) {
        prev_ticks_.resize(cur.size() * 4);
        for (size_t i = 0; i < cur.size(); ++i)
            for (size_t s = 0; s < 4 && s < cur[i].size(); ++s) prev_ticks_[i * 4 + s] = cur[i][s];
        initialized_ = true;
        last_sample_time_ = now_seconds();
        return;
    }

    snapshot.cpu.per_core_percent.clear();
    snapshot.cpu.per_core_percent.reserve(cur.size());
    double sum_used = 0;
    for (size_t i = 0; i < cur.size(); ++i) {
        std::vector<unsigned int> prev(4);
        for (size_t s = 0; s < 4; ++s) prev[s] = prev_ticks_[i * 4 + s];
        const double pct = pct_from_ticks(cur[i], prev);
        snapshot.cpu.per_core_percent.push_back(pct);
        sum_used += pct;
        for (size_t s = 0; s < 4 && s < cur[i].size(); ++s) prev_ticks_[i * 4 + s] = cur[i][s];
    }

    const int cores = std::max(1, static_cast<int>(cur.size()));
    const double avg = sum_used / cores;
    snapshot.cpu.user_percent = avg * 0.75;
    snapshot.cpu.system_percent = avg * 0.25;
    snapshot.cpu.idle_percent = std::max(0.0, 100.0 - avg);
    snapshot.cpu.p_core_percent = avg;  // refined by thermal/powermetrics later
    snapshot.cpu.e_core_percent = avg * 0.5;
    snapshot.has_cpu = true;
    last_sample_time_ = now_seconds();
}

}  // namespace marrow
