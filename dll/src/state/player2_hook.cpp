#include "player2_hook.h"

#include <windows.h>
#include <MinHook.h>

#include <cstdlib>

#include "../logging.h"
#include "player2.h"
#include "globals.h"

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

    // Optional spawn-shift: env TH08_PLATFORM_PLAYER2_X_OFFSET=N moves
    // g_Player2 by N pixels in X right after init. Useful for VISUAL
    // confirmation that two players exist (without it they overlap).
    char xoff_env[16] = {};
    if (GetEnvironmentVariableA("TH08_PLATFORM_PLAYER2_X_OFFSET", xoff_env,
                                static_cast<DWORD>(sizeof(xoff_env))) > 0) {
        const float shift = static_cast<float>(atof(xoff_env));
        if (shift != 0.0f) {
            *reinterpret_cast<float*>(p2 + 0x3D8) += shift;
            // Also nudge the hitbox AABB the same way so collision math
            // (when 5c lands) sees the shifted position.
            *reinterpret_cast<float*>(p2 + 0x3BC) += shift;
            *reinterpret_cast<float*>(p2 + 0x3C8) += shift;
            const float new_x = *reinterpret_cast<float*>(p2 + 0x3D8);
            th08_platform::log_line("player2: shifted X by %.2f -> new pos.x=%.2f", shift, new_x);
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
    static unsigned int log_interval = 0;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        char env[16] = {};
        if (GetEnvironmentVariableA("TH08_PLATFORM_PLAYER2_TICK_LOG", env,
                                    static_cast<DWORD>(sizeof(env))) > 0) {
            int n = atoi(env);
            log_interval = (n > 0) ? static_cast<unsigned int>(n) : 0;
        }
    }
    if (log_interval == 0 || !g_player2_done) return;
    if (frame_no % log_interval != 0) return;

    auto dump = [&](const char* tag, std::uint8_t* p) {
        const int st = static_cast<int>(p[0]);
        const float pa_x = *reinterpret_cast<float*>(p + 0x3D4);
        const float pa_y = *reinterpret_cast<float*>(p + 0x3E0);
        const float pb_x = *reinterpret_cast<float*>(p + 0x3D8);
        const float pb_y = *reinterpret_cast<float*>(p + 0x3E4);
        const float hb_min_x = *reinterpret_cast<float*>(p + 0x3BC);
        const float hb_min_y = *reinterpret_cast<float*>(p + 0x3C0);
        const float hb_max_x = *reinterpret_cast<float*>(p + 0x3C8);
        const float hb_max_y = *reinterpret_cast<float*>(p + 0x3CC);
        th08_platform::log_line(
            "%s frame=%llu state=%d posA=(%.2f,%.2f) posB=(%.2f,%.2f) hb=[(%.2f,%.2f),(%.2f,%.2f)]",
            tag, frame_no, st, pa_x, pa_y, pb_x, pb_y,
            hb_min_x, hb_min_y, hb_max_x, hb_max_y);
    };
    // p1: read g_Player (the singleton ZUN constructed at 0x17D5EF8).
    // p2: read our buffer. Compare drift over time:
    //   - if posA drifts each frame and posB stays put -> A=current, B=target
    //   - if posB drifts and posA stays put            -> opposite
    //   - if hitbox starts at [0,0] and becomes non-zero -> hitbox is set
    //     by per-frame movement code, NOT by AddedCallback.
    dump("p1-tick", reinterpret_cast<std::uint8_t*>(globals::kAddr_g_Player));
    dump("p2-tick", th08_platform::player2::g_Player2);
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
