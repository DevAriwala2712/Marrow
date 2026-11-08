#include "sysscope/provider_factory.hpp"

#include <chrono>
#include <cstdio>
#include <string_view>
#include <thread>

namespace {

void print_json_snapshot(const sysscope::MetricsSnapshot& s) {
    std::printf("{\"timestamp\":%.3f", s.timestamp);
    if (s.has_cpu)
        std::printf(",\"cpu\":{\"user\":%.2f,\"system\":%.2f}", s.cpu.user_percent, s.cpu.system_percent);
    if (s.has_memory)
        std::printf(",\"memory\":{\"used\":%llu,\"total\":%llu}",
                    static_cast<unsigned long long>(s.memory.used_bytes),
                    static_cast<unsigned long long>(s.memory.total_bytes));
    if (s.has_network)
        std::printf(",\"network\":{\"in_bps\":%.0f,\"out_bps\":%.0f}",
                    s.network.bytes_in_per_sec, s.network.bytes_out_per_sec);
    if (s.has_disk)
        std::printf(",\"disk\":{\"read_bps\":%.0f,\"write_bps\":%.0f}",
                    s.disk.read_bytes_per_sec, s.disk.write_bytes_per_sec);
    if (s.has_thermal)
        std::printf(",\"thermal\":{\"pressure\":\"%s\"}", s.thermal.pressure.c_str());
    std::puts("}");
}

void print_prometheus(const sysscope::MetricsSnapshot& s) {
    if (s.has_cpu) {
        std::puts("# HELP sysscope_cpu_user_percent CPU user percent");
        std::puts("# TYPE sysscope_cpu_user_percent gauge");
        std::printf("sysscope_cpu_user_percent %.2f\n", s.cpu.user_percent);
    }
    if (s.has_memory) {
        std::puts("# HELP sysscope_memory_used_bytes Physical memory used in bytes");
        std::puts("# TYPE sysscope_memory_used_bytes gauge");
        std::printf("sysscope_memory_used_bytes %llu\n",
                    static_cast<unsigned long long>(s.memory.used_bytes));
    }
}

constexpr const char* usage = "Usage: marrow-cli [--format json|prometheus] | ping | --version | --help";

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2) {
        const std::string_view option = argv[1];
        if (option == "--help" || option == "-h") {
            std::puts(usage);
            return 0;
        }
        if (option == "--version" || option == "ping") {
            std::puts("Marrow " MARROW_VERSION);
            return 0;
        }
    }

    std::string_view format = "json";
    if (argc == 3 && std::string_view(argv[1]) == "--format") format = argv[2];
    else if (argc != 1) format = {};
    if (format != "json" && format != "prometheus") {
        std::fprintf(stderr, "%s\n", usage);
        return 2;
    }

    sysscope::MetricAggregator aggregator;
    aggregator.set_providers(sysscope::make_real_providers());
    aggregator.collect();
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    const auto snapshot = aggregator.collect();
    aggregator.stop();

    if (format == "prometheus") print_prometheus(snapshot);
    else print_json_snapshot(snapshot);
    return 0;
}
