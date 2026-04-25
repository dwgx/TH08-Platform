#include "dual_collision.h"

#include <windows.h>
#include <MinHook.h>

#include <atomic>

#include "../logging.h"
#include "globals.h"
#include "player2.h"

namespace th08_platform::state::dual_collision {

namespace {

// Player::TestHitbox @ 0x449FF0 (verified by decompile, not in
// mapping.csv with this name). Convention: __thiscall(Player* this,
// float* bullet_pos, int bullet_meta) -> int (2 = hit, 0 = miss).
constexpr DWORD kTestHitboxRVA = 0x449ff0 - 0x400000;

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
// unused — we accept it as a placeholder param.
using TestHitbox_t   = int(__fastcall*)(void* this_, void* edx_unused,
                                        float* bullet_pos, int bullet_meta);
using PlayerDeath_t  = void*(__fastcall*)(void* this_, void* edx_unused);

TestHitbox_t g_original = nullptr;

std::atomic<unsigned long long> g_p1_hits{0};
std::atomic<unsigned long long> g_p2_hits{0};
std::atomic<unsigned long long> g_total_calls{0};

int __fastcall hooked_TestHitbox(void* this_, void* edx, float* bullet_pos, int bullet_meta)
{
    g_total_calls.fetch_add(1, std::memory_order_relaxed);

    // Step 1: run the original against whatever Player was passed.
    // In practice this is always &g_Player (from BulletManager etc.);
    // our hook only diverges if it's specifically that.
    const int r1 = g_original(this_, edx, bullet_pos, bullet_meta);
    if (r1 == 2) {
        if (this_ == reinterpret_cast<void*>(globals::kAddr_g_Player)) {
            g_p1_hits.fetch_add(1, std::memory_order_relaxed);
        }
        return r1;
    }

    // Step 2: if original missed AND the test was for g_Player, try
    // again with g_Player2 (only if it's been constructed).
    if (this_ != reinterpret_cast<void*>(globals::kAddr_g_Player)) {
        return r1;  // someone else's hitbox test, don't double-up
    }
    if (!th08_platform::player2::IsConstructed()) {
        return r1;
    }

    // Re-run with g_Player2 as `this`. If it hits, the side-effect
    // writes (hit-info fields, hit-count increment) land on g_Player2,
    // not g_Player. Damage attribution is correct.
    void* const p2 = th08_platform::player2::g_Player2;
    const int r2 = g_original(p2, edx, bullet_pos, bullet_meta);
    if (r2 == 2) {
        g_p2_hits.fetch_add(1, std::memory_order_relaxed);

        // Manually trigger the death sequence on g_Player2. Without
        // this, BulletManager's caller chain never knows P2 was the
        // one hit (TestHitbox returns just an int). The chain calls
        // death code with `&g_Player` baked in, so P2 absorbs hits
        // silently (no state=2, no anim, no sfx). sub_44AB40 is
        // this-aware: it sets state=2, plays the death anim at
        // this+0x2B4 (pos), plays the sfx, and queues the respawn-init
        // path that 5f's hook then routes to our P2 lives counter.
        // Wrapped in SEH because we're calling into ZUN code with
        // a non-canonical Player ptr (our DLL buffer).
        auto* const trigger_death =
            reinterpret_cast<PlayerDeath_t>(
                reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr))
                + kPlayerDeathRVA);
        __try {
            trigger_death(p2, nullptr);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            log_line("dual_collision: SEH during P2 death trigger; P2 may be in inconsistent state");
        }
    }
    return r2;
}

void* resolve_target()
{
    HMODULE base = GetModuleHandleA(nullptr);
    if (!base) return nullptr;
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uint8_t*>(base) + kTestHitboxRVA);
}

}  // namespace

bool install_hook()
{
    // MH_Initialize is owned by install_game_loop_hook (called first).
    void* target = resolve_target();
    if (!target) {
        log_line("dual_collision: resolve_target returned null");
        return false;
    }
    log_line("dual_collision: hooking Player::TestHitbox @ %p", target);

    if (MH_CreateHook(target, reinterpret_cast<void*>(&hooked_TestHitbox),
                      reinterpret_cast<LPVOID*>(&g_original)) != MH_OK) {
        log_line("dual_collision: MH_CreateHook failed");
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        log_line("dual_collision: MH_EnableHook failed");
        return false;
    }
    log_line("dual_collision: enabled (env-gated, dual P1/P2 tests live)");
    return true;
}

void uninstall_hook()
{
    if (g_original == nullptr) return;
    void* target = resolve_target();
    if (target != nullptr) {
        MH_DisableHook(target);
        MH_RemoveHook(target);
    }
    g_original = nullptr;
}

HitCounters snapshot_hit_counters()
{
    HitCounters c;
    c.p1_hits = g_p1_hits.load(std::memory_order_relaxed);
    c.p2_hits = g_p2_hits.load(std::memory_order_relaxed);
    c.total_calls = g_total_calls.load(std::memory_order_relaxed);
    return c;
}

}  // namespace th08_platform::state::dual_collision
