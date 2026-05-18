#include "sysscope/providers/process_provider.hpp"
#include "sysscope/util.hpp"

#include <libproc.h>
#include <sys/sysctl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace sysscope {

namespace {

const char* task_status_string(int state) {
    switch (state) {
        case 1: return "running";
        case 2: return "sleeping";
        case 3: return "waiting";
        case 4: return "stopped";
        case 5: return "zombie";
        default: return "unknown";
    }
}

}  // namespace

void ProcessProvider::tick(MetricsSnapshot& snapshot) {
    int capacity = 4096;
    std::vector<pid_t> pids(static_cast<size_t>(capacity));
    int bytes = proc_listpids(PROC_ALL_PIDS, 0, pids.data(),
                              static_cast<int>(pids.size() * sizeof(pid_t)));
    if (bytes <= 0) return;

    int count = bytes / static_cast<int>(sizeof(pid_t));
    if (count > capacity) {
        capacity = count + 64;
        pids.resize(static_cast<size_t>(capacity));
        bytes = proc_listpids(PROC_ALL_PIDS, 0, pids.data(),
                              static_cast<int>(pids.size() * sizeof(pid_t)));
        if (bytes <= 0) return;
        count = bytes / static_cast<int>(sizeof(pid_t));
    }

    const double now = now_seconds();
    const double wall_dt = (last_wall_ > 0) ? (now - last_wall_) : 0;
    last_wall_ = now;

    std::vector<ProcessNode> nodes;
    nodes.reserve(static_cast<size_t>(count));

    for (int i = 0; i < count; ++i) {
        const pid_t pid = pids[static_cast<size_t>(i)];
        if (pid <= 0) continue;

        struct proc_taskinfo ti {};
        if (proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &ti, sizeof(ti)) <= 0) continue;

        char name[PROC_PIDPATHINFO_MAXSIZE] = {};
        proc_name(pid, name, sizeof(name));

        struct proc_bsdinfo bi {};
        proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bi, sizeof(bi));

        double cpu_pct = 0;
        if (wall_dt > 0.01) {
            const auto it = prev_.find(pid);
            if (it != prev_.end() && it->second.at > 0) {
                static int ncpu = 0;
                if (ncpu == 0) {
                    size_t sz = sizeof(ncpu);
                    sysctlbyname("hw.logicalcpu", &ncpu, &sz, nullptr, 0);
                    if (ncpu < 1) ncpu = 1;
                }
                const double duser = static_cast<double>(ti.pti_total_user - it->second.user_ns);
                const double dsys = static_cast<double>(ti.pti_total_system - it->second.sys_ns);
                const double dcpu = (duser + dsys) / 1e9;
                cpu_pct = std::min(100.0 * dcpu / (wall_dt * ncpu), 999.0);
            }
        }
        prev_[pid] = {ti.pti_total_user, ti.pti_total_system, now};

        ProcessNode node;
        node.id = pid;
        node.name = name[0] ? name : "unknown";
        node.parent_pid = bi.pbi_ppid;
        node.mem_bytes = ti.pti_resident_size;
        node.cpu_percent = cpu_pct;
        node.status = task_status_string(bi.pbi_status);
        nodes.push_back(std::move(node));
    }

    std::sort(nodes.begin(), nodes.end(),
              [](const ProcessNode& a, const ProcessNode& b) { return a.cpu_percent > b.cpu_percent; });
    if (nodes.size() > 256) nodes.resize(256);

    snapshot.process_graph.nodes = std::move(nodes);
    snapshot.has_process_graph = true;
}

}  // namespace sysscope
