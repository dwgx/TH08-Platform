#include "dual_collision.h"

#include <windows.h>
#include <MinHook.h>

#include <atomic>
#include <cstdint>

#include "../logging.h"
#include "globals.h"
#include "player2.h"

namespace th08_platform::state::dual_collision {

namespace {

// Player::TestHitbox @ 0x449FF0 (worksheet Section 1).
// Convention: __thiscall(Player* this, float* bullet_pos, int bullet_meta)
// -> int (2 = hit, 0 = miss).
constexpr DWORD kTestHitboxRVA = 0x449ff0 - 0x400000;
constexpr DWORD kBulletBodyRVA = 0x44a230 - 0x400000;
constexpr DWORD kBulletGrazeRVA = 0x44a470 - 0x400000;
constexpr DWORD kCalcLaserHitboxRVA = 0x44a6a0 - 0x400000;

// Player death-trigger sub_44AB40 (verified this-aware via direct IDA
// decomp). __thiscall(Player*) -> ptr. Writes state=2, plays death
// anim+sfx at this+0x2B4, etc. Without manually calling this for
// g_Player2 when 5c reports a P2 hit, BulletManager's caller never
// triggers the death sequence (it only calls death code with
// &g_Player baked in). End result: P2 absorbs hits silently.
constexpr DWORD kPlayerDeathRVA = 0x44ab40 - 0x400000;

// MSVC won't let us declare a detour function as `__thiscall`, so we
// use `__fastcall` which has the same ABI for the first two args (ECX,
// EDX). Original takes only one register arg (this in ECX), so EDX is
// unused; we accept it as a placeholder param.
using TestHitbox_t   = int(__fastcall*)(void* this_, void* edx_unused,
                                        float* bullet_pos, int bullet_meta);
using PlayerDeath_t  = void*(__fastcall*)(void* this_, void* edx_unused);
using BulletBody_t    = int(__fastcall*)(void* this_, void* edx_unused, void* bullet);
using BulletGraze_t   = int(__fastcall*)(void* this_, void* edx_unused, void* bullet);
using LaserHitbox_t   = int(__fastcall*)(void* this_, void* edx_unused,
                                         float* laser_pos, int meta,
                                         float* laser_size, float angle,
                                         int allow_graze);

TestHitbox_t g_orig_test_hitbox = nullptr;
BulletBody_t g_orig_bullet_body = nullptr;
BulletGraze_t g_orig_bullet_graze = nullptr;
LaserHitbox_t g_orig_laser_hitbox = nullptr;

// Re-entrancy guard. The wrapper hooks (sub_44A230 / sub_44A470 /
// CalcLaserHitbox) call the original wrapper internally, and the original
// wrapper internally calls Player::TestHitbox. Without this guard, the
// inner TestHitbox call would mirror P2 again, causing a 3x raw TestHitbox
// invocation per state-case-1 bullet (P1+P2 from outer wrapper mirror, plus
// nested P2 from inner TestHitbox mirror). This drops the game from 60 FPS
// to 30 FPS on bullet-heavy frames. With the guard, inner TestHitbox calls
// pass through to the original without mirroring (the outer wrapper has
// already arranged a P2 call). thread_local because hot path runs on
// game's main thread but DLL design must not assume it.
thread_local bool g_in_wrapper_mirror = false;

struct WrapperMirrorGuard {
    WrapperMirrorGuard() noexcept { g_in_wrapper_mirror = true; }
    ~WrapperMirrorGuard() noexcept { g_in_wrapper_mirror = false; }
};

std::atomic<unsigned long long> g_p1_hits{0};
std::atomic<unsigned long long> g_p2_hits{0};
std::atomic<unsigned long long> g_total_calls{0};       // raw hooked_TestHitbox invocations
std::atomic<unsigned long long> g_calls_44A230{0};      // raw hooked_44A230 invocations
std::atomic<unsigned long long> g_calls_44A470{0};      // raw hooked_44A470 invocations
std::atomic<unsigned long long> g_calls_laser{0};       // raw hooked_CalcLaserHitbox invocations
std::atomic<unsigned long long> g_44A230_p1_hits{0};
std::atomic<unsigned long long> g_44A230_p2_hits{0};
std::atomic<unsigned long long> g_44A470_p1_grazes{0};
std::atomic<unsigned long long> g_44A470_p2_grazes{0};
std::atomic<unsigned long long> g_laser_p1_hits{0};
std::atomic<unsigned long long> g_laser_p2_hits{0};

inline bool is_p1(void* p) noexcept
{
    return p == reinterpret_cast<void*>(globals::kAddr_g_Player);
}

inline bool should_route_to_p2(void* p) noexcept
{
    return is_p1(p) && th08_platform::player2::IsEligible();
}

inline void* p2() noexcept
{
    return th08_platform::player2::g_Player2;
}

void* resolve(DWORD rva)
{
    HMODULE base = GetModuleHandleA(nullptr);
    if (!base) return nullptr;
    return reinterpret_cast<void*>(reinterpret_cast<std::uint8_t*>(base) + rva);
}

int merge_2_1_0(int r1, int r2) noexcept
{
    if (r1 == 2 || r2 == 2) return 2;
    if (r1 == 1 || r2 == 1) return 1;
    return 0;
}

void count_testhitbox_attribution(std::atomic<unsigned long long>& counter) noexcept
{
    counter.fetch_add(1, std::memory_order_relaxed);
}

bool call_test_hitbox(void* this_, void* edx, float* bullet_pos, int bullet_meta,
                      int* out_result) noexcept
{
    __try {
        *out_result = g_orig_test_hitbox(this_, edx, bullet_pos, bullet_meta);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_line("dual_collision: SEH in Player::TestHitbox original (this=%p)", this_);
        *out_result = 0;
        return false;
    }
}

bool call_bullet_body(void* this_, void* edx, void* bullet, int* out_result) noexcept
{
    __try {
        *out_result = g_orig_bullet_body(this_, edx, bullet);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_line("dual_collision: SEH in sub_44A230 original (this=%p)", this_);
        *out_result = 0;
        return false;
    }
}

bool call_bullet_graze(void* this_, void* edx, void* bullet, int* out_result) noexcept
{
    __try {
        *out_result = g_orig_bullet_graze(this_, edx, bullet);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_line("dual_collision: SEH in sub_44A470 original (this=%p)", this_);
        *out_result = 0;
        return false;
    }
}

bool call_laser_hitbox(void* this_, void* edx, float* laser_pos, int meta,
                       float* laser_size, float angle, int allow_graze,
                       int* out_result) noexcept
{
    __try {
        *out_result = g_orig_laser_hitbox(this_, edx, laser_pos, meta, laser_size,
                                          angle, allow_graze);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_line("dual_collision: SEH in Player::CalcLaserHitbox original (this=%p)", this_);
        *out_result = 0;
        return false;
    }
}

void trigger_p2_death(void* player) noexcept
{
    auto* const trigger_death = reinterpret_cast<PlayerDeath_t>(resolve(kPlayerDeathRVA));
    if (!trigger_death) {
        log_line("dual_collision: could not resolve sub_44AB40 for P2 death");
        return;
    }

    __try {
        trigger_death(player, nullptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_line("dual_collision: SEH during P2 death trigger; P2 may be in inconsistent state");
    }
}

// Audit: sub_430830 calls TestHitbox twice for an active bullet record; only
// the second return is tested against 2. On hit it calls sub_4400A0 once with
// dword_18B8988/type 1, then clears that bullet record with memset. On miss it
// either marks state 5 (a2 == 4) or runs the alternate single conversion path.
// sub_430AA0 calls TestHitbox once per active bullet; return == 2 selects one
// conversion argument, after which popup/score conversion and state=5 happen
// exactly once. Neither caller receives a Player* from TestHitbox, and neither
// branches on which player was hit. Returning 2 for any P1/P2 hit therefore
// consumes each projectile once, with no player-attribution-dependent spend.
int __fastcall hooked_TestHitbox(void* this_, void* edx, float* bullet_pos, int bullet_meta)
{
    g_total_calls.fetch_add(1, std::memory_order_relaxed);
    int r1 = 0;
    if (!call_test_hitbox(this_, edx, bullet_pos, bullet_meta, &r1)) {
        return 0;
    }

    // Re-entrancy: this call is nested inside a wrapper-hook mirror block
    // (the outer wrapper has already arranged its own P2 mirror call via
    // the original wrapper). Don't mirror P2 again — that would cause a
    // 3x TestHitbox call blowup. Just pass through.
    if (g_in_wrapper_mirror) {
        return r1;
    }

    if (!is_p1(this_)) {
        return r1;
    }

    if (r1 == 2) {
        count_testhitbox_attribution(g_p1_hits);
    }

    if (!th08_platform::player2::IsEligible()) {
        return r1;
    }

    int r2 = 0;
    call_test_hitbox(p2(), edx, bullet_pos, bullet_meta, &r2);
    if (r2 == 2) {
        count_testhitbox_attribution(g_p2_hits);
        trigger_p2_death(p2());
    }
    return (r1 == 2 || r2 == 2) ? 2 : r1;
}

int __fastcall hooked_44A230(void* this_, void* edx, void* bullet)
{
    g_calls_44A230.fetch_add(1, std::memory_order_relaxed);
    WrapperMirrorGuard guard;  // suppresses nested TestHitbox mirror
    int r1 = 0;
    if (!call_bullet_body(this_, edx, bullet, &r1)) {
        return 0;
    }
    if (is_p1(this_) && r1 != 0) {
        g_44A230_p1_hits.fetch_add(1, std::memory_order_relaxed);
    }
    if (!should_route_to_p2(this_)) {
        return r1;
    }

    int r2 = 0;
    call_bullet_body(p2(), edx, bullet, &r2);
    if (r2 != 0) {
        g_44A230_p2_hits.fetch_add(1, std::memory_order_relaxed);
    }
    return merge_2_1_0(r1, r2);
}

// Graze policy is intentionally naive: each eligible player who grazes the
// projectile gets the vanilla sub_44A930 side effects from the original call.
// No per-projectile dedup is applied.
int __fastcall hooked_44A470(void* this_, void* edx, void* bullet)
{
    g_calls_44A470.fetch_add(1, std::memory_order_relaxed);
    WrapperMirrorGuard guard;
    int r1 = 0;
    if (!call_bullet_graze(this_, edx, bullet, &r1)) {
        return 0;
    }
    if (is_p1(this_) && r1 == 1) {
        g_44A470_p1_grazes.fetch_add(1, std::memory_order_relaxed);
    }
    if (!should_route_to_p2(this_)) {
        return r1;
    }

    int r2 = 0;
    call_bullet_graze(p2(), edx, bullet, &r2);
    if (r2 == 1) {
        g_44A470_p2_grazes.fetch_add(1, std::memory_order_relaxed);
    }
    return merge_2_1_0(r1, r2);
}

// One hook covers all BulletManager laser-loop and ECL wrapper call sites in
// the worksheet. The original applies direct-hit death and graze effects
// internally for whichever Player* it receives.
int __fastcall hooked_CalcLaserHitbox(void* this_, void* edx, float* laser_pos,
                                      int meta, float* laser_size, float angle,
                                      int allow_graze)
{
    g_calls_laser.fetch_add(1, std::memory_order_relaxed);
    WrapperMirrorGuard guard;
    int r1 = 0;
    if (!call_laser_hitbox(this_, edx, laser_pos, meta, laser_size, angle,
                           allow_graze, &r1)) {
        return 0;
    }
    if (is_p1(this_) && r1 == 1) {
        g_laser_p1_hits.fetch_add(1, std::memory_order_relaxed);
    }
    if (!should_route_to_p2(this_)) {
        return r1;
    }

    int r2 = 0;
    call_laser_hitbox(p2(), edx, laser_pos, meta, laser_size, angle,
                      allow_graze, &r2);
    if (r2 == 1) {
        g_laser_p2_hits.fetch_add(1, std::memory_order_relaxed);
    }
    return merge_2_1_0(r1, r2);
}

void cleanup_hooks(void* t_test, void* t_body, void* t_graze, void* t_laser) noexcept
{
    if (g_orig_test_hitbox && t_test) {
        MH_DisableHook(t_test);
        MH_RemoveHook(t_test);
    }
    if (g_orig_bullet_body && t_body) {
        MH_DisableHook(t_body);
        MH_RemoveHook(t_body);
    }
    if (g_orig_bullet_graze && t_graze) {
        MH_DisableHook(t_graze);
        MH_RemoveHook(t_graze);
    }
    if (g_orig_laser_hitbox && t_laser) {
        MH_DisableHook(t_laser);
        MH_RemoveHook(t_laser);
    }
    g_orig_test_hitbox = nullptr;
    g_orig_bullet_body = nullptr;
    g_orig_bullet_graze = nullptr;
    g_orig_laser_hitbox = nullptr;
}

}  // namespace

bool install_hook()
{
    // MH_Initialize is owned by install_game_loop_hook (called first).
    void* t_test = resolve(kTestHitboxRVA);
    void* t_body = resolve(kBulletBodyRVA);
    void* t_graze = resolve(kBulletGrazeRVA);
    void* t_laser = resolve(kCalcLaserHitboxRVA);
    if (!t_test || !t_body || !t_graze || !t_laser) {
        log_line("dual_collision: resolve failed (%p %p %p %p)",
                 t_test, t_body, t_graze, t_laser);
        return false;
    }
    log_line("dual_collision: hooking TestHitbox @ %p, sub_44A230 @ %p, sub_44A470 @ %p, CalcLaserHitbox @ %p",
             t_test, t_body, t_graze, t_laser);

    MH_STATUS status = MH_CreateHook(t_test, reinterpret_cast<void*>(&hooked_TestHitbox),
                                     reinterpret_cast<LPVOID*>(&g_orig_test_hitbox));
    if (status != MH_OK) {
        log_line("dual_collision: MH_CreateHook TestHitbox failed status=%d", status);
        return false;
    }
    status = MH_CreateHook(t_body, reinterpret_cast<void*>(&hooked_44A230),
                           reinterpret_cast<LPVOID*>(&g_orig_bullet_body));
    if (status != MH_OK) {
        log_line("dual_collision: MH_CreateHook sub_44A230 failed status=%d", status);
        cleanup_hooks(t_test, t_body, t_graze, t_laser);
        return false;
    }
    status = MH_CreateHook(t_graze, reinterpret_cast<void*>(&hooked_44A470),
                           reinterpret_cast<LPVOID*>(&g_orig_bullet_graze));
    if (status != MH_OK) {
        log_line("dual_collision: MH_CreateHook sub_44A470 failed status=%d", status);
        cleanup_hooks(t_test, t_body, t_graze, t_laser);
        return false;
    }
    status = MH_CreateHook(t_laser, reinterpret_cast<void*>(&hooked_CalcLaserHitbox),
                           reinterpret_cast<LPVOID*>(&g_orig_laser_hitbox));
    if (status != MH_OK) {
        log_line("dual_collision: MH_CreateHook CalcLaserHitbox failed status=%d", status);
        cleanup_hooks(t_test, t_body, t_graze, t_laser);
        return false;
    }

    status = MH_EnableHook(t_test);
    if (status != MH_OK) {
        log_line("dual_collision: MH_EnableHook TestHitbox failed status=%d", status);
        cleanup_hooks(t_test, t_body, t_graze, t_laser);
        return false;
    }
    status = MH_EnableHook(t_body);
    if (status != MH_OK) {
        log_line("dual_collision: MH_EnableHook sub_44A230 failed status=%d", status);
        cleanup_hooks(t_test, t_body, t_graze, t_laser);
        return false;
    }
    status = MH_EnableHook(t_graze);
    if (status != MH_OK) {
        log_line("dual_collision: MH_EnableHook sub_44A470 failed status=%d", status);
        cleanup_hooks(t_test, t_body, t_graze, t_laser);
        return false;
    }
    status = MH_EnableHook(t_laser);
    if (status != MH_OK) {
        log_line("dual_collision: MH_EnableHook CalcLaserHitbox failed status=%d", status);
        cleanup_hooks(t_test, t_body, t_graze, t_laser);
        return false;
    }

    log_line("dual_collision: enabled (v2: TestHitbox + bullet body/graze + laser mirrors)");
    return true;
}

void uninstall_hook()
{
    cleanup_hooks(resolve(kTestHitboxRVA), resolve(kBulletBodyRVA),
                  resolve(kBulletGrazeRVA), resolve(kCalcLaserHitboxRVA));
}

HitCounters snapshot_hit_counters()
{
    HitCounters c;
    c.total_calls       = g_total_calls.load(std::memory_order_relaxed);
    c.calls_44A230      = g_calls_44A230.load(std::memory_order_relaxed);
    c.calls_44A470      = g_calls_44A470.load(std::memory_order_relaxed);
    c.calls_laser       = g_calls_laser.load(std::memory_order_relaxed);
    c.p1_hits           = g_p1_hits.load(std::memory_order_relaxed);
    c.p2_hits           = g_p2_hits.load(std::memory_order_relaxed);
    c.p1_44A230_hits    = g_44A230_p1_hits.load(std::memory_order_relaxed);
    c.p2_44A230_hits    = g_44A230_p2_hits.load(std::memory_order_relaxed);
    c.p1_44A470_grazes  = g_44A470_p1_grazes.load(std::memory_order_relaxed);
    c.p2_44A470_grazes  = g_44A470_p2_grazes.load(std::memory_order_relaxed);
    c.p1_laser_hits     = g_laser_p1_hits.load(std::memory_order_relaxed);
    c.p2_laser_hits     = g_laser_p2_hits.load(std::memory_order_relaxed);
    return c;
}

}  // namespace th08_platform::state::dual_collision
