#pragma once

// Sub-phase 5a: g_Player2 allocation skeleton.
//
// Provides a second 928560-byte (0xE2B30) buffer that mirrors ZUN's
// singleton g_Player at 0x17D5EF8. Construct() runs the original
// Player::Player ctor (0x449ca0) on our buffer via a __thiscall thunk.
//
// 5a does NOT register g_Player2 with g_Chain. That is sub-phase 5b's
// job - we don't want to start ticking g_Player2's OnUpdate until we
// understand whether ZUN's chain dispatcher tolerates two Player entries.
// Until then, Construct() just produces a constructed-but-inert second
// Player block that we can read/write in tests without the game running
// it.
//
// Lifecycle:
//   - g_Player2 is a static aligned buffer in this TU (no heap alloc).
//   - Construct() is idempotent; calling twice will re-zero and re-ctor.
//   - Destruct() is a placeholder; ZUN's Player has no explicit dtor in
//     mapping.csv (only DeletedCallback @ 0x44dc60 which frees the two
//     shtFile slot heap pointers). 5a's Construct() never triggers
//     AddedCallback, so no heap is allocated, so Destruct() is no-op.

#include <cstddef>
#include <cstdint>

namespace th08_platform::player2 {

inline constexpr std::size_t kSize = 0xE2B30;  // sizeof(Player), verified 2026-04-25

// The buffer itself. 16-byte aligned to match what ZUN's binary expects
// for any inline D3DXVECTOR3 / float[4] members. Public so test code
// can inspect/seed bytes directly.
extern alignas(16) std::uint8_t g_Player2[kSize];

// Returns true if the buffer was successfully ctor'd. Writes a log line
// either way. Safe to call multiple times.
bool Construct();

// Currently no-op. Provided so call sites have a symmetric API for when
// 5b wires chain de-registration.
void Destruct();

// True iff Construct() has been called and returned true since the last
// Destruct(). Used by Register() and 5b to gate chain registration.
bool IsConstructed() noexcept;

// Registers g_Player2 with ZUN's update + draw chains, mirroring what
// Player::RegisterChain (0x44C230) does for g_Player.
//
// HAZARD: once registered, g_Chain.Step() will tick g_Player2's OnUpdate
// every frame. OnUpdate calls helpers (sub_44C5B0 etc.) that may
// dereference fields populated only by Player::AddedCallback (e.g. the
// shtFile heap pointers, the primary weapon AnmVm). Calling Register()
// WITHOUT also driving AddedCallback for g_Player2 will likely crash on
// first frame.
//
// Sub-phase 5b will pair Register() with a deferred AddedCallback path.
// For 5a, Register() is provided so the wiring is in place, but it is
// NOT auto-invoked - opt-in via env TH08_PLATFORM_TEST_PLAYER2=2.
//
// Returns true if all three chain inserts succeeded. On failure, calls
// Unregister() to clean up partial state.
bool Register();

// Removes g_Player2 from the chains. Safe to call when not registered.
void Unregister();

// True iff the last Register() call succeeded and Unregister() hasn't run.
bool IsRegistered() noexcept;

}  // namespace th08_platform::player2
