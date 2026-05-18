#pragma once

#include "marrow/ipc.hpp"
#include "marrow/types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace marrow {

std::string encode_request(const XpcRequest& req);
std::string encode_response(const XpcResponse& resp);
std::optional<XpcRequest> decode_request(const std::string& json);
std::optional<XpcResponse> decode_response(const std::string& json);

std::string default_socket_path();

/// Length-prefixed frame: [uint32_be size][utf8 json]
bool write_frame(int fd, const std::string& payload);
std::optional<std::string> read_frame(int fd);

}  // namespace marrow
