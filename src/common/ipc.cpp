#include "sysscope/ipc.hpp"
#include "sysscope/stub_providers.hpp"

namespace sysscope {

bool HelperClient::connect() {
    aggregator_.set_providers(make_stub_providers());
    connected_ = true;
    return true;
}

void HelperClient::disconnect() {
    if (connected_) aggregator_.stop();
    connected_ = false;
}

XpcResponse HelperClient::send(const XpcRequest& req) {
    XpcResponse resp;
    switch (req.kind) {
        case XpcRequestKind::Ping:
            resp.kind = XpcResponseKind::Pong;
            resp.version = "0.1.0-cpp-stub";
            break;
        case XpcRequestKind::FetchSnapshot:
            resp.kind = XpcResponseKind::Snapshot;
            resp.snapshot = aggregator_.collect();
            break;
        default:
            resp.kind = XpcResponseKind::Ack;
            break;
    }
    return resp;
}

}  // namespace sysscope
