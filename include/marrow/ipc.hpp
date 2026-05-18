#pragma once

#include "marrow/metric_provider.hpp"
#include "marrow/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace marrow {

enum class XpcRequestKind {
    Ping,
    StartCollection,
    StopCollection,
    FetchSnapshot,
    FetchHistory,
    SubscribeMetrics,
    UninstallHelper
};

enum class XpcResponseKind { Pong, Snapshot, History, Error, SubscriptionStarted, Ack };

struct XpcRequest {
    XpcRequestKind kind = XpcRequestKind::Ping;
};

struct XpcResponse {
    XpcResponseKind kind = XpcResponseKind::Ack;
    std::string version;
    MetricsSnapshot snapshot;
};

/// Typed IPC scaffold (Unix socket / helper daemon). Stub: in-process only for now.
class HelperClient {
public:
    bool connect();
    void disconnect();
    XpcResponse send(const XpcRequest& req);
    bool use_stubs = true;

private:
    MetricAggregator aggregator_;
    int socket_fd_ = -1;
    bool connected_ = false;
};

}  // namespace marrow
