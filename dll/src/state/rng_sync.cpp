#include "rng_sync.h"

#include "../logging.h"
#include "../net/lockstep.h"

#include <windows.h>
#include <cstdint>

namespace th08_platform::state::rng_sync {

namespace {
constexpr std::uintptr_t kRngStateAddr = 0x0164D520;
constexpr std::uintptr_t kRngCounterAddr = 0x0164D524;

bool g_applied = false;
}

bool apply_if_ready()
{
    if (g_applied) return false;
    if (!th08_platform::net::has_shared_seed()) return false;

    const auto seed = th08_platform::net::shared_seed();
    const auto state16 = static_cast<std::uint16_t>(seed & 0xFFFF);

    auto* state = reinterpret_cast<std::uint16_t*>(kRngStateAddr);
    auto* counter = reinterpret_cast<std::uint32_t*>(kRngCounterAddr);
    *state = state16;
    *counter = 0;

    th08_platform::log_line(
        "phase 6c: applied shared seed at game start: 0x%04x (full=0x%08x)",
        state16, seed);
    g_applied = true;
    return true;
}

bool has_been_applied()
{
    return g_applied;
}

}  // namespace th08_platform::state::rng_sync
