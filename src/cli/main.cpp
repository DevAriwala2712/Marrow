#include "sysscope/ipc.hpp"

#include <cstdio>
#include <cstring>
#include <string>

static void print_json_snapshot(const sysscope::MetricsSnapshot& s) {
    std::printf("{\"timestamp\":%.3f", s.timestamp);
    if (s.has_cpu)
        std::printf(",\"cpu\":{\"user\":%.2f,\"system\":%.2f}", s.cpu.user_percent, s.cpu.system_percent);
    if (s.has_memory)
        std::printf(",\"memory\":{\"used\":%llu,\"total\":%llu}",
                    static_cast<unsigned long long>(s.memory.used_bytes),
                    static_cast<unsigned long long>(s.memory.total_bytes));
    std::printf("}\n");
}

static void print_prometheus(const sysscope::MetricsSnapshot& s) {
    if (s.has_cpu) {
        std::printf("# HELP sysscope_cpu_user_percent CPU user percent\n");
        std::printf("# TYPE sysscope_cpu_user_percent gauge\n");
        std::printf("sysscope_cpu_user_percent %.2f\n", s.cpu.user_percent);
    }
    if (s.has_memory)
        std::printf("sysscope_memory_used_bytes %llu\n",
                    static_cast<unsigned long long>(s.memory.used_bytes));
}

int main(int argc, char** argv) {
    std::string format = "json";
    bool ping_only = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "ping") == 0) ping_only = true;
        else if (std::strcmp(argv[i], "--format") == 0 && i + 1 < argc) format = argv[++i];
    }

    sysscope::HelperClient client;
    if (!client.connect()) {
        std::fprintf(stderr, "sysscope: connect failed\n");
        return 1;
    }

    if (ping_only) {
        auto resp = client.send({sysscope::XpcRequestKind::Ping});
        std::printf("%s\n", resp.version.c_str());
        return 0;
    }

    auto resp = client.send({sysscope::XpcRequestKind::FetchSnapshot});
    if (resp.kind != sysscope::XpcResponseKind::Snapshot) {
        std::fprintf(stderr, "sysscope: unexpected response\n");
        return 1;
    }
    if (format == "prometheus") print_prometheus(resp.snapshot);
    else print_json_snapshot(resp.snapshot);
    return 0;
}
