#pragma once

#include "sysscope/metric_provider.hpp"
#include "sysscope/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace sysscope {

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
    bool connected_ = false;
};

}  // namespace sysscope
