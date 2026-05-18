#include "sysscope/ipc.hpp"
#include "sysscope/stub_providers.hpp"

#include <chrono>
#include <cstdio>
#include <thread>
#include <unistd.h>

/// Privileged helper scaffold (SMJobBless / Mach service wiring TBD).
int main() {
    sysscope::MetricAggregator aggregator;
    aggregator.set_providers(sysscope::make_stub_providers());
    std::fprintf(stderr, "SysScopeHelper stub running (PID %d)\n", getpid());
    for (;;) {
        auto snap = aggregator.collect();
        std::fprintf(stderr, "metrics ts=%.0f cpu=%.1f%%\n", snap.timestamp,
                     snap.has_cpu ? snap.cpu.total_used() : 0.0);
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    return 0;
}
