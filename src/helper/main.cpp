#include "marrow/helper_server.hpp"
#include "marrow/metrics_codec.hpp"
#include "marrow/provider_factory.hpp"

#include <csignal>
#include <cstdio>
#include <unistd.h>

static marrow::HelperServer* g_server = nullptr;

static void on_signal(int) {
    if (g_server) g_server->stop();
}

int main() {
    marrow::MetricAggregator aggregator;
    aggregator.set_providers(marrow::make_real_providers());

    marrow::HelperServer server(marrow::default_socket_path());
    g_server = &server;
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    if (!server.start(aggregator)) {
        std::fprintf(stderr, "MarrowHelper: failed to bind %s\n",
                     marrow::default_socket_path().c_str());
        return 1;
    }

    std::fprintf(stderr, "MarrowHelper running (PID %d) socket=%s\n", getpid(),
                 marrow::default_socket_path().c_str());

    while (true) pause();
    return 0;
}
