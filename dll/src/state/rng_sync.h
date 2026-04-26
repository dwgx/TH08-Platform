#pragma once

#include <cstdint>

namespace th08_platform::state::rng_sync {

// Apply the shared seed (from net::shared_seed()) to th08's g_Rng. Idempotent
// after first call. Safe to call from hooked_OnUpdate at frame==1.
// Returns true if a seed was applied this call (i.e., first time + has_shared_seed).
bool apply_if_ready();

// True after apply_if_ready() has succeeded once.
bool has_been_applied();

}  // namespace th08_platform::state::rng_sync
