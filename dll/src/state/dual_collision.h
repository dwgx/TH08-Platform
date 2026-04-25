#pragma once

// Sub-phase 5i: dual-player bullet/laser routing.
//
// Env-gated by TH08_PLATFORM_DUAL_COLLISION=1. Option A was chosen for
// Phase 5i: this env var now installs the v2 four-hook bundle, replacing
// the old v1 P1-first fallback behavior.
//
// Installed hooks:
//   - Player::TestHitbox @ 0x449FF0
//   - sub_44A230 bullet body/death wrapper
//   - sub_44A470 bullet graze/near-hit wrapper
//   - Player::CalcLaserHitbox @ 0x44A6A0
//
// Every hook routes only when ECX is the canonical &g_Player and
// player2::IsEligible() is true. Calls with any other Player pointer are
// passed through to the original without recursive routing. P2 TestHitbox
// death attribution still manually invokes sub_44AB40(g_Player2), because
// some caller chains only know how to kill the canonical g_Player.
//
// Independent of TH08_PLATFORM_PLAYER2_AUTO; both must be set for dual
// collision to be useful, since the mirror path is a no-op until g_Player2
// exists and is eligible.

namespace th08_platform::state::dual_collision {

bool install_hook();
void uninstall_hook();

// Diagnostic: per-hook P1/P2 attribution + raw invocation counts since last
// reset. Logged via on_frame_tick. total_calls is the raw TestHitbox hook
// invocation count (incremented every call, regardless of hit), so a value
// of 0 unambiguously means the hook is not being triggered. The four
// _calls fields likewise count raw invocations of each hook.
struct HitCounters {
    // raw hook invocation counts (every call to the detour, whether hit or miss)
    unsigned long long total_calls;        // Player::TestHitbox detour
    unsigned long long calls_44A230;       // sub_44A230 detour (bullet body)
    unsigned long long calls_44A470;       // sub_44A470 detour (graze)
    unsigned long long calls_laser;        // CalcLaserHitbox detour

    // per-hook hit/graze attribution
    unsigned long long p1_hits;            // TestHitbox r1==2 attributed to P1
    unsigned long long p2_hits;            // TestHitbox r2==2 attributed to P2
    unsigned long long p1_44A230_hits;
    unsigned long long p2_44A230_hits;
    unsigned long long p1_44A470_grazes;
    unsigned long long p2_44A470_grazes;
    unsigned long long p1_laser_hits;
    unsigned long long p2_laser_hits;
};
HitCounters snapshot_hit_counters();

}  // namespace th08_platform::state::dual_collision
