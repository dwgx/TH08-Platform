#pragma once

#include <cstdint>
#include <optional>

namespace th08_platform::net {

bool initialize(const char* peer_spec, std::uint16_t listen_port);
void shutdown();

bool is_configured();
bool is_connected();
void poll();
void send_local_input(std::uint64_t frame, std::uint16_t input);
std::optional<std::uint16_t> wait_for_peer_input(std::uint64_t frame,
                                                 std::uint32_t timeout_ms);

}  // namespace th08_platform::net
