#pragma once

// Sub-phase 5h: mirror hardcoded-g_Player calls to also fire for
// g_Player2. Codex 5.5 round-2 RE found three subsystems where ZUN's
// engine code passes &g_Player as `this` directly (hardcoded
// `mov ecx, offset byte_17D5EF8; call X` instructions in callers).
// For each, our DLL detours the callee. When invoked with this==g_Player,
// we ALSO invoke original with this=g_Player2 so the per-player work
// happens for both:
//
//   sub_44DE60 / sub_44DF00 (hit-slot writers): enemy code calls
//     these to insert "active bullet at (x,y) with radius R" into
//     the player's hit-slot table at this+0xBB834. Without mirroring,
//     g_Player2's slots stay empty -> 5c's TestHitbox re-test always
//     misses -> P2 never visibly takes damage. Mirroring populates
//     g_Player2's slots in lock-step with g_Player's.
//
//   sub_451670 (player-bullet vs enemy damage check): EnemyManager
//     iterates this->bullet_array (128 slots at this+0xBE838) and
//     applies damage to enemies for each hit. Three call sites in
//     EnemyManager (0x42C76E / 0x42D0D5 / 0x42D122) pass &g_Player
//     hardcoded. Without mirroring, P2's bullets render but never
//     damage enemies. Mirroring lets g_Player2's bullets also damage.
//
// Bomb subsystem fix lives in player2_hook.cpp's Register() — it's a
// one-shot memcpy of g_Player+0x1000..0x1014 (the bomb-handler function
// pointer table) into g_Player2's same offset. Without that, g_Player2's
// table is zero -> the function-pointer dispatch at 0x44CB1A crashes
// or no-ops when P2 bombs.
//
// Default off; opt-in via env TH08_PLATFORM_P2_MIRROR=1. Independent
// of other Phase 5 envs but only useful with PLAYER2_AUTO=1 and
// DUAL_COLLISION=1 active.

namespace th08_platform::state::p2_mirror {

bool install();
void uninstall();

}  // namespace th08_platform::state::p2_mirror
