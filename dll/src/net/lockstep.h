#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace th08_platform::net {

// Phase 4 legacy view of a confirmed remote frame. Kept so rollback.cpp
// and frame_state.cpp compile; under the Phase 6b Pack-based wire the
// new accessors below are preferred.
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

// Drain incoming UDP, dispatch packets, send periodic PING. Called by the
// pump thread (`pump_thread.cpp`) every ~16ms. Safe to call when not
// configured (returns immediately).
void poll();

// Phase 6b additions. Driven from `hooks/input.cpp`'s GetInput hook.
void capture_local_input(std::uint64_t frame, std::uint16_t input);
void send_input_pack_if_due(std::uint64_t frame);
std::uint16_t peek_remote_input(std::uint64_t frame);
std::uint64_t last_rtt_ms();
bool has_shared_seed();
std::uint32_t shared_seed();
void send_ghost_pack(std::uint64_t frame, float pos_x, float pos_y,
                     std::uint16_t lives, std::uint16_t bombs,
                     std::uint16_t power, std::uint32_t score);
bool peek_peer_ghost(float& out_x, float& out_y, std::uint64_t& out_frame);
std::uint16_t peer_ghost_lives();
std::uint16_t peer_ghost_bombs();
std::uint16_t peer_ghost_power();
std::uint32_t peer_ghost_score();

// Phase 4 legacy entry points. Under the new Pack wire format these
// degrade to no-ops / sensible defaults; rollback.cpp keeps building
// but actual remote-frame plumbing now goes through capture_local_input
// / peek_remote_input above. Phase 6g will rewire rollback to those.
void send_local_input(std::uint64_t frame, std::uint16_t input, std::uint32_t checksum);
std::optional<ConfirmedFrame> peek_confirmed_frame(std::uint64_t frame);
std::vector<ConfirmedFrame> consume_confirmed_frames_up_to(std::uint64_t frame);
std::uint16_t last_known_remote_input();

}  // namespace th08_platform::net
