#pragma once

// Sub-phase 5c: dual-player collision.
//
// Hooks `Player::TestHitbox @ 0x449FF0` (mapping.csv NOT named, but
// xref + decompile analysis: __thiscall(Player*, float* bullet_pos, int
// bullet_meta), iterates 192 hitbox slots at this+0xBB7B4, returns 2
// on hit). All bullet / laser / item systems route through this
// primitive — hooking it once covers BulletManager::OnUpdate (3 call
// sites), LaserManager (sub_430830 / sub_430AA0), and Player's own
// shot tests (sub_44A230 / sub_44A470).
//
// Detour logic:
//   1. Call original with whatever Player is in ECX (normally &g_Player).
//   2. If hit (returns 2), let the result propagate so the original
//      caller updates that Player's hit-info fields.
//   3. If miss AND the ECX argument was &g_Player AND g_Player2 is
//      registered, call the original AGAIN with this=&g_Player2. If
//      that hits, the return-2 makes the caller think P1 got hit, but
//      the side-effect writes (*(this+232100) = v8[10]; ++v8[12])
//      land on g_Player2's slots — so P2 absorbs the damage and P1
//      stays clean. That's the correct behavior IF the caller doesn't
//      do further per-player work outside the hit-info fields.
//
// Default: hook is NOT installed. Opt-in via env
//   TH08_PLATFORM_DUAL_COLLISION=1
// (independent of TH08_PLATFORM_PLAYER2_AUTO; both must be set for
// dual collision to actually be useful, since the hook is a no-op when
// IsConstructed() is false).

namespace th08_platform::state::dual_collision {

bool install_hook();
void uninstall_hook();

// Diagnostic: total #hits attributed to P1 / P2 since last reset.
// Logged each second via on_frame_tick (gated by same env as 5b's
// per-frame log). Useful to confirm bullets really are testing both
// players in real play.
struct HitCounters {
    unsigned long long p1_hits;
    unsigned long long p2_hits;
    unsigned long long total_calls;
};
HitCounters snapshot_hit_counters();

}  // namespace th08_platform::state::dual_collision
