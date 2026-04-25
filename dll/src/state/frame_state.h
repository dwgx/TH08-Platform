#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace th08_platform::state {

struct CapturedRegion {
    std::uintptr_t address = 0;
    std::size_t size = 0;
    std::vector<std::uint8_t> bytes;
    const char* label = nullptr;
};

struct FrameState {
    std::uint64_t frame_number = 0;
    std::vector<CapturedRegion> regions;
    std::array<std::uint8_t, 108> fpu{};
};

void capture(FrameState& state, std::uint64_t frame_number);
void restore(const FrameState& state);
std::size_t total_payload_bytes(const FrameState& state) noexcept;

}  // namespace th08_platform::state
