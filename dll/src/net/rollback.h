#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../state/frame_state.h"

namespace th08_platform::net::rollback {

constexpr std::size_t kMaxRollbackFrames = 16;

struct RollbackBuffer {
    std::array<state::FrameState, kMaxRollbackFrames> history;
    std::uint64_t confirmed_frame = 0;
    std::uint64_t local_frame = 0;
    std::size_t head = 0;
};

bool install_audio_hooks();
void reset();
void on_frame_start(std::uint64_t frame_number);
void on_frame_end(void* game_manager, std::uint64_t frame_number);
bool is_replaying();

}  // namespace th08_platform::net::rollback
