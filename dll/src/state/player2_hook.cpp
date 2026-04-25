#include "player2_hook.h"

#include <windows.h>
#include <MinHook.h>

#include "../logging.h"
#include "player2.h"

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

bool g_player2_done = false;  // idempotent guard

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
