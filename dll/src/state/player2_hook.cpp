#include "player2_hook.h"

#include <windows.h>
#include <MinHook.h>

#include <cstdlib>

#include "../logging.h"
#include "player2.h"
#include "globals.h"
#include "p2_lives.h"
#include "dual_collision.h"
#include "item_routing.h"

namespace th08_platform::state {

namespace {

// th08::Player::RegisterChain @ 0x44C230 in th08.exe v1.00d.
// mapping.csv row 762:
//   th08::Player::RegisterChain,0x44c230,0x15f,__fastcall,,ZunResult,u8
// __fastcall with one u8 arg: the byte arrives in CL (low ECX). The
// function memsets g_Player to zero, allocates 3 ChainElems for the
// real Player, and returns ZUN_SUCCESS (0) on success.
constexpr DWORD kRegisterChainRVA = 0x44c230 - 0x400000;

using RegisterChain_t = int(__fastcall*)(unsigned char arg);
RegisterChain_t g_original_RegisterChain = nullptr;

}  // namespace

bool g_player2_done = false;  // idempotent guard, exposed for on_frame_tick

namespace {

int __fastcall hooked_RegisterChain(unsigned char arg)
{
    th08_platform::log_line("player2_hook: Player::RegisterChain(0x%02X) intercepted", arg);
    const int result = g_original_RegisterChain(arg);
    th08_platform::log_line("player2_hook: original Player::RegisterChain returned %d", result);

    if (result != 0) {
        th08_platform::log_line("player2_hook: original failed; skipping g_Player2 setup");
        return result;
    }

    if (g_player2_done) {
        th08_platform::log_line("player2_hook: already set up (re-init?); skipping");
        return result;
    }

    th08_platform::log_line("player2_hook: now constructing + registering g_Player2");
    if (!th08_platform::player2::Construct()) {
        th08_platform::log_line("player2_hook: g_Player2 Construct failed; aborting auto-register");
        return result;
    }
    if (!th08_platform::player2::Register()) {
        th08_platform::log_line("player2_hook: g_Player2 Register failed; ctor stays alive but inert");
        return result;
    }

    g_player2_done = true;
    th08_platform::log_line("player2_hook: g_Player2 fully wired (ctor + chain)");

    // Sub-phase 5h fix for bombs (codex 5.5 round-2 RE):
    // sub_44C650's bomb logic dispatches via a function-pointer table
    // at this+0x1000 (5 entries, indexed by bomb type). For g_Player
    // this table is populated by ZUN's init code AFTER AddedCallback;
    // for our g_Player2 (also AddedCallback'd) the SAME init runs but
    // it might key off character data we don't fully replicate. The
    // safe shortcut: snapshot g_Player's table now (after BOTH
    // AddedCallbacks have run) and copy it to g_Player2.
    {
        auto* const p1_tab = reinterpret_cast<void**>(
            globals::kAddr_g_Player + 0x1000);
        auto* const p2_tab = reinterpret_cast<void**>(
            reinterpret_cast<std::uintptr_t>(th08_platform::player2::g_Player2) + 0x1000);
        for (int i = 0; i < 5; ++i) {
            p2_tab[i] = p1_tab[i];
        }
        th08_platform::log_line("player2: copied bomb-handler table 0x1000..0x1014 from g_Player to g_Player2 (entries: %p %p %p %p %p)",
                                p1_tab[0], p1_tab[1], p1_tab[2], p1_tab[3], p1_tab[4]);
    }

    // Post-Register diagnostic: did Chain::AddToCalcChain auto-call
    // Player::AddedCallback? If yes, playerState should be 1
    // (PLAYER_STATE_SPAWNING) since AddedCallback writes that. If still
    // 0 (PLAYER_STATE_ALIVE from ctor's memset), AddedCallback wasn't
    // called and we need to invoke it explicitly for full init.
    auto* const p2 = th08_platform::player2::g_Player2;
    const int post_state = static_cast<int>(p2[0]);
    const float pos_x = *reinterpret_cast<float*>(p2 + 0x3D8);
    const float pos_y = *reinterpret_cast<float*>(p2 + 0x3E4);
    const float hb_min_x = *reinterpret_cast<float*>(p2 + 0x3BC);
    const float hb_max_x = *reinterpret_cast<float*>(p2 + 0x3C8);
    th08_platform::log_line("player2: post-Register state=%d pos=(%.2f, %.2f) hitbox.x=[%.2f, %.2f]",
                            post_state, pos_x, pos_y, hb_min_x, hb_max_x);

    // Spawn-shift: TH08_PLATFORM_PLAYER2_X_OFFSET=N pixels.
    // Verified by IDA RE (codex 5e batch): live pos.x/y is at
    // +0x2B4/+0x2B8 (not +0x3D4/+0x3D8 which is some unrelated
    // static field). The boundary clamp at 0x44D420 reads from
    // +0x2B4 (X) and +0x2B8 (Y) and compares to play-area edges.
    // Without this shift, BOTH players draw at the same world pos
    // and P2 is occluded by P1's sprite (= what user observed in
    // the runtime test).
    // Default 80px shift so P2 isn't drawn on top of P1 at spawn.
    // Override with TH08_PLATFORM_PLAYER2_X_OFFSET=N (set =0 to disable).
    char xoff_env[16] = {};
    float shift = 80.0f;
    if (GetEnvironmentVariableA("TH08_PLATFORM_PLAYER2_X_OFFSET", xoff_env,
                                static_cast<DWORD>(sizeof(xoff_env))) > 0) {
        shift = static_cast<float>(atof(xoff_env));
    }
    if (shift != 0.0f) {
        *reinterpret_cast<float*>(p2 + 0x2B4) += shift;          // pos.x
        // Also nudge the hitbox AABB the same way so collision math
        // sees the shifted position right away (the per-frame
        // movement code in FUN_0044AEC0 will overwrite hitbox
        // values from current pos, so this only matters for the
        // first frame before that runs).
        *reinterpret_cast<float*>(p2 + 0x3BC) += shift;
        *reinterpret_cast<float*>(p2 + 0x3C8) += shift;
        const float new_x = *reinterpret_cast<float*>(p2 + 0x2B4);
        th08_platform::log_line("player2: shifted X by %.2f -> new pos.x=%.2f", shift, new_x);
    }
    char yoff_env[16] = {};
    if (GetEnvironmentVariableA("TH08_PLATFORM_PLAYER2_Y_OFFSET", yoff_env,
                                static_cast<DWORD>(sizeof(yoff_env))) > 0) {
        const float shift = static_cast<float>(atof(yoff_env));
        if (shift != 0.0f) {
            *reinterpret_cast<float*>(p2 + 0x2B8) += shift;          // pos.y
            *reinterpret_cast<float*>(p2 + 0x3C0) += shift;
            *reinterpret_cast<float*>(p2 + 0x3CC) += shift;
            const float new_y = *reinterpret_cast<float*>(p2 + 0x2B8);
            th08_platform::log_line("player2: shifted Y by %.2f -> new pos.y=%.2f", shift, new_y);
        }
    }

    return result;
}

void* resolve_target()
{
    HMODULE base = GetModuleHandleA(nullptr);
    if (!base) return nullptr;
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uint8_t*>(base) + kRegisterChainRVA);
}

}  // namespace

bool install_player2_hook()
{
    // MH_Initialize is owned by install_game_loop_hook (called first).
    void* target = resolve_target();
    if (!target) {
        th08_platform::log_line("player2_hook: resolve_target returned null");
        return false;
    }
    th08_platform::log_line("player2_hook: hooking Player::RegisterChain at %p", target);

    if (MH_CreateHook(target, reinterpret_cast<void*>(&hooked_RegisterChain),
                      reinterpret_cast<LPVOID*>(&g_original_RegisterChain)) != MH_OK) {
        th08_platform::log_line("player2_hook: MH_CreateHook failed");
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        th08_platform::log_line("player2_hook: MH_EnableHook failed");
        return false;
    }
    th08_platform::log_line("player2_hook: Player::RegisterChain hook enabled");
    return true;
}

void on_frame_tick(unsigned long long frame_no)
{
    // Default 60-frame (= 1 second @ 60fps) tick log. Override with
    // TH08_PLATFORM_PLAYER2_TICK_LOG=N (set =0 to disable diagnostic).
    static unsigned int log_interval = 60;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        char env[16] = {};
        if (GetEnvironmentVariableA("TH08_PLATFORM_PLAYER2_TICK_LOG", env,
                                    static_cast<DWORD>(sizeof(env))) > 0) {
            int n = atoi(env);
            log_interval = (n >= 0) ? static_cast<unsigned int>(n) : 60;
        }
    }
    if (log_interval == 0 || !g_player2_done) return;
    if (frame_no % log_interval != 0) return;

    auto dump = [&](const char* tag, std::uint8_t* p) {
        // Verified offsets:
        //   +0x2B4 / +0x2B8 = live pos.x / pos.y (float, codex 5e)
        //   +0x38C / +0x390 / +0x398 / +0x39C = body AABB used by
        //   TestHitbox sub_449FF0 (codex 5g-recon corrected this -
        //   my earlier guess of +0x3BC was wrong - that's some other
        //   per-frame field, not the hit-detection AABB)
        const int st = static_cast<int>(p[0]);
        const float pos_x = *reinterpret_cast<float*>(p + 0x2B4);
        const float pos_y = *reinterpret_cast<float*>(p + 0x2B8);
        const float aabb_min_x = *reinterpret_cast<float*>(p + 0x38C);
        const float aabb_min_y = *reinterpret_cast<float*>(p + 0x390);
        const float aabb_max_x = *reinterpret_cast<float*>(p + 0x398);
        const float aabb_max_y = *reinterpret_cast<float*>(p + 0x39C);
        const float aabb_w = aabb_max_x - aabb_min_x;
        const float aabb_h = aabb_max_y - aabb_min_y;
        // Also peek the first hit-test slot at +0xBB834 (active flag at
        // +60, center at +0/+4, radius at +8). Tells us if slots are
        // populated (P2 should see same values as P1 if writer is
        // this-aware AND running for both).
        const std::uint8_t slot0_active =
            *reinterpret_cast<std::uint8_t*>(p + 0xBB834 + 60);
        const float slot0_cx = *reinterpret_cast<float*>(p + 0xBB834);
        const float slot0_cy = *reinterpret_cast<float*>(p + 0xBB834 + 4);
        const float slot0_r  = *reinterpret_cast<float*>(p + 0xBB834 + 8);
        th08_platform::log_line(
            "%s f=%llu st=%d pos=(%.1f,%.1f) aabb=%.1fx%.1f@(%.1f,%.1f) slot0[active=%d c=(%.1f,%.1f) r=%.1f]",
            tag, frame_no, st, pos_x, pos_y, aabb_w, aabb_h,
            (aabb_min_x + aabb_max_x) * 0.5f,
            (aabb_min_y + aabb_max_y) * 0.5f,
            static_cast<int>(slot0_active), slot0_cx, slot0_cy, slot0_r);
    };
    // p1: read g_Player (the singleton ZUN constructed at 0x17D5EF8).
    // p2: read our buffer.
    dump("p1-tick", reinterpret_cast<std::uint8_t*>(globals::kAddr_g_Player));
    dump("p2-tick", th08_platform::player2::g_Player2);

    // Phase 5g preview: log-based "HUD" showing per-player stats.
    // Real visual HUD pending IDA recon of AsciiManager API.
    const int p2_lives = th08_platform::state::p2_lives::snapshot_p2_lives();
    const auto hits = th08_platform::state::dual_collision::snapshot_hit_counters();
    const auto items = th08_platform::state::item_routing::snapshot_counters();
    const unsigned long long redirects =
        th08_platform::state::p2_lives::snapshot_redirect_count();
    th08_platform::log_line(
        "HUD frame=%llu | P2 lives=%d | redirs=%llu | calls: TH=%llu BB=%llu GZ=%llu LZ=%llu | "
        "TH p1/p2=%llu/%llu | BB p1/p2=%llu/%llu | GZ p1/p2=%llu/%llu | LZ p1/p2=%llu/%llu | "
        "IT p1/p2=%llu/%llu calls=%llu | IA p1/p2=%llu/%llu | AT p1/p2=%llu/%llu",
        frame_no, p2_lives, redirects,
        hits.total_calls, hits.calls_44A230, hits.calls_44A470, hits.calls_laser,
        hits.p1_hits, hits.p2_hits,
        hits.p1_44A230_hits, hits.p2_44A230_hits,
        hits.p1_44A470_grazes, hits.p2_44A470_grazes,
        hits.p1_laser_hits, hits.p2_laser_hits,
        items.routed_p1, items.routed_p2, items.routed_calls,
        items.attracted_p1, items.attracted_p2,
        items.trigger_p1, items.trigger_p2);
}

void uninstall_player2_hook()
{
    if (g_original_RegisterChain == nullptr) return;
    void* target = resolve_target();
    if (target != nullptr) {
        MH_DisableHook(target);
        MH_RemoveHook(target);
    }
    if (g_player2_done) {
        th08_platform::player2::Unregister();
        th08_platform::player2::Destruct();
        g_player2_done = false;
    }
    g_original_RegisterChain = nullptr;
}

}  // namespace th08_platform::state
