#include "sysscope/helper_server.hpp"
#include "sysscope/metrics_codec.hpp"
#include "sysscope/provider_factory.hpp"

#include <csignal>
#include <cstdio>
#include <unistd.h>

static sysscope::HelperServer* g_server = nullptr;

static void on_signal(int) {
    if (g_server) g_server->stop();
}

int main() {
    sysscope::MetricAggregator aggregator;
    aggregator.set_providers(sysscope::make_real_providers());

    sysscope::HelperServer server(sysscope::default_socket_path());
    g_server = &server;
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    if (!server.start(aggregator)) {
        std::fprintf(stderr, "SysScopeHelper: failed to bind %s\n",
                     sysscope::default_socket_path().c_str());
        return 1;
    }

    std::fprintf(stderr, "SysScopeHelper running (PID %d) socket=%s\n", getpid(),
                 sysscope::default_socket_path().c_str());

    while (true) pause();
    return 0;
}
