#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace th08_platform::net {

struct ConfirmedFrame {
    std::uint32_t frame = 0;
    std::uint16_t input = 0;
    std::uint32_t checksum = 0;
};

bool initialize(const char* peer_spec, std::uint16_t listen_port);
bool initialize_listen_only(std::uint16_t listen_port);
void shutdown();

void set_fake_rtt_ms(std::uint32_t fake_rtt_ms);
bool is_configured();
bool is_connected();
void poll();
void send_local_input(std::uint64_t frame, std::uint16_t input, std::uint32_t checksum);
std::optional<ConfirmedFrame> peek_confirmed_frame(std::uint64_t frame);
std::vector<ConfirmedFrame> consume_confirmed_frames_up_to(std::uint64_t frame);
std::uint16_t last_known_remote_input();

}  // namespace th08_platform::net
