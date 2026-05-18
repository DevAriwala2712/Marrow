#pragma once

#include "marrow/metric_provider.hpp"
#include "marrow/types.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace marrow {

class HelperServer {
public:
    explicit HelperServer(std::string socket_path);
    ~HelperServer();

    bool start(MetricAggregator& aggregator);
    void stop();

    MetricsSnapshot latest_snapshot() const;

private:
    void accept_loop();
    void client_loop(int client_fd);
    void collection_loop(MetricAggregator* aggregator);

    std::string socket_path_;
    std::thread accept_thread_;
    std::thread collection_thread_;
    std::atomic<bool> running_{false};
    int listen_fd_ = -1;

    mutable std::mutex snapshot_mutex_;
    MetricsSnapshot latest_;
};

}  // namespace marrow
