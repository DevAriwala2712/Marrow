#include "sysscope/helper_server.hpp"
#include "sysscope/metrics_codec.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <filesystem>

namespace sysscope {

HelperServer::HelperServer(std::string socket_path) : socket_path_(std::move(socket_path)) {}

HelperServer::~HelperServer() { stop(); }

bool HelperServer::start(MetricAggregator& aggregator) {
    if (running_) return true;
    std::filesystem::create_directories(std::filesystem::path(socket_path_).parent_path());
    ::unlink(socket_path_.c_str());

    listen_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd_ < 0) return false;

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);
    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }
    listen(listen_fd_, 8);
    running_ = true;
    collection_thread_ = std::thread([this, &aggregator] { collection_loop(&aggregator); });
    accept_thread_ = std::thread([this] { accept_loop(); });
    return true;
}

void HelperServer::stop() {
    running_ = false;
    if (listen_fd_ >= 0) {
        close(listen_fd_);
        listen_fd_ = -1;
    }
    ::unlink(socket_path_.c_str());
    if (collection_thread_.joinable()) collection_thread_.join();
    if (accept_thread_.joinable()) accept_thread_.join();
}

MetricsSnapshot HelperServer::latest_snapshot() const {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    return latest_;
}

void HelperServer::collection_loop(MetricAggregator* aggregator) {
    while (running_) {
        MetricsSnapshot snap = aggregator->collect();
        {
            std::lock_guard<std::mutex> lock(snapshot_mutex_);
            latest_ = snap;
        }
        usleep(1000 * 1000);
    }
}

void HelperServer::accept_loop() {
    while (running_) {
        const int client = accept(listen_fd_, nullptr, nullptr);
        if (client < 0) {
            if (running_) usleep(100000);
            continue;
        }
        std::thread(&HelperServer::client_loop, this, client).detach();
    }
}

void HelperServer::client_loop(int client_fd) {
    while (running_) {
        const auto frame = read_frame(client_fd);
        if (!frame) break;
        const auto req = decode_request(*frame);
        if (!req) break;

        XpcResponse resp;
        switch (req->kind) {
            case XpcRequestKind::Ping:
                resp.kind = XpcResponseKind::Pong;
                resp.version = "0.2.0-cpp-helper";
                break;
            case XpcRequestKind::FetchSnapshot:
                resp.kind = XpcResponseKind::Snapshot;
                resp.snapshot = latest_snapshot();
                break;
            default:
                resp.kind = XpcResponseKind::Ack;
                break;
        }
        if (!write_frame(client_fd, encode_response(resp))) break;
    }
    close(client_fd);
}

}  // namespace sysscope
