#pragma once

// Sub-phase 5f: per-player lives counter.
//
// ZunGlobals.livesRemaining (at *(g_GameManager+0x08) + 0x74) is a
// SINGLETON in ZUN's design. Without this hook, when g_Player2 dies,
// the respawn handler decrements that singleton -> P1 visibly loses a
// life when P2 took the hit. That breaks the "real 2P" promise.
//
// Verified by IDA RE (codex 5e batch puzzle 5):
//   Player::FUN_0044cbf0 @ 0x44CBF0 is the respawn-init function.
//   Its body contains:
//     if (unknown_libname_2(this + 928500) >= 30) {
//       *this = 1;                               // state -> SPAWNING
//       if (sub_42F2B0(&dword_160F508) > 0) {
//         sub_43C641(&dword_160F508, -1);        // AddLives(-1)
//         ...
//   sub_43C641 is GameManager::AddLives at 0x43C641 (already
//   100%-byte-matched in the decomp via our binary patcher Phase 1a).
//
// Strategy: TWO hooks coordinated via a thread-local-ish flag:
//   1. Hook 0x44CBF0 (FUN_0044cbf0). Detour: set g_p2_dying = true
//      when called with this==g_Player2; run original; clear flag.
//   2. Hook 0x43C641 (AddLives). Detour: if g_p2_dying AND delta<0,
//      skip the original write to ZunGlobals and instead decrement
//      our g_p2_lives counter. Otherwise pass through.
//
// Other callers of AddLives (extend bonus, game-over reset, etc.)
// run unaffected because g_p2_dying is only set during the brief
// window of FUN_0044cbf0(g_Player2).
//
// Default off; opt-in via env TH08_PLATFORM_P2_LIVES=1.

#include <cstdint>

namespace th08_platform::state::p2_lives {

bool install_hook();
void uninstall_hook();

// Snapshot the current P2 life count (for HUD code in 5g).
int snapshot_p2_lives() noexcept;

// Diagnostic: how many "P2 took a death" events have we redirected.
unsigned long long snapshot_redirect_count() noexcept;

}  // namespace th08_platform::state::p2_lives
