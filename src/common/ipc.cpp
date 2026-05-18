#include "sysscope/ipc.hpp"
#include "sysscope/metrics_codec.hpp"
#include "sysscope/provider_factory.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>

namespace sysscope {

namespace {

int connect_socket(const std::string& path) {
    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

std::optional<XpcResponse> transact(int fd, const XpcRequest& req) {
    if (!write_frame(fd, encode_request(req))) return std::nullopt;
    const auto frame = read_frame(fd);
    if (!frame) return std::nullopt;
    return decode_response(*frame);
}

}  // namespace

bool HelperClient::connect() {
    disconnect();
    const int fd = connect_socket(default_socket_path());
    if (fd >= 0) {
        socket_fd_ = fd;
        connected_ = true;
        use_stubs = false;
        return true;
    }
    aggregator_.set_providers(make_stub_providers());
    connected_ = true;
    use_stubs = true;
    return true;
}

void HelperClient::disconnect() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    if (connected_ && use_stubs) aggregator_.stop();
    connected_ = false;
}

XpcResponse HelperClient::send(const XpcRequest& req) {
    if (!connected_) {
        XpcResponse err;
        err.kind = XpcResponseKind::Ack;
        return err;
    }
    if (!use_stubs && socket_fd_ >= 0) {
        if (auto resp = transact(socket_fd_, req)) return *resp;
        disconnect();
        connect();
        if (!use_stubs && socket_fd_ >= 0) {
            if (auto resp = transact(socket_fd_, req)) return *resp;
        }
    }
    XpcResponse resp;
    switch (req.kind) {
        case XpcRequestKind::Ping:
            resp.kind = XpcResponseKind::Pong;
            resp.version = use_stubs ? "0.1.0-cpp-stub" : "0.2.0-cpp-helper";
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
