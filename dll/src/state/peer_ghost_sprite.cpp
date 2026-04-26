#include "peer_ghost_sprite.h"

#include "../logging.h"
#include "../net/lockstep.h"

#include <windows.h>
#include <MinHook.h>

#include <cstdint>

namespace th08_platform::state::peer_ghost_sprite {

namespace {

// th08::Player::OnDrawHighPrio @ 0x44D530 (mapping.csv line 772, 0xF1 bytes).
// __fastcall(Player*) -> ChainCallbackResult (returns int).
constexpr DWORD kPlayer_OnDrawHighPrio_RVA = 0x44D530 - 0x400000;

// Embedded AnmVm inside Player: Player+0x10 (verified in
// th08-decomp/src/Player.cpp:67 -- ExecuteScript((AnmVm *)((u8 *)player + 0x10))).
// AnmVm prefix is 0x208 bytes (C_ASSERT in AnmManager.hpp), pos.x/y at
// offset 0x208/0x20C from the AnmVm = 0x218/0x21C from Player base.
constexpr std::ptrdiff_t kOffset_Player_AnmVm_PosX = 0x10 + 0x208;
constexpr std::ptrdiff_t kOffset_Player_AnmVm_PosY = 0x10 + 0x20C;

// AnmManager helpers used to make the second draw look "ghosted" — a
// translucent blue-gray tint so users can tell the network ghost from the
// local player at a glance. The AnmManager pointer lives at
// 0x018BDC90 (reccmp-symbols.csv:2340). SetMixColor /
// SetMixColorDefault are __thiscall and addressed by absolute, since
// they live in TH08's image (RVA + image base 0x400000).
constexpr DWORD kSetMixColor_RVA        = 0x40BAD0 - 0x400000;
constexpr DWORD kSetMixColorDefault_RVA = 0x40BAB0 - 0x400000;
constexpr std::uintptr_t kAddr_g_AnmManager_ptr = 0x018BDC90;

using SetMixColor_t        = void(__thiscall*)(void*, std::uint32_t /*D3DCOLOR*/);
using SetMixColorDefault_t = void(__thiscall*)(void*);

SetMixColor_t        g_SetMixColor        = nullptr;
SetMixColorDefault_t g_SetMixColorDefault = nullptr;

// 0x80 = 50% alpha, RGB = warm pinkish overlay so the ghost reads as
// "another player" rather than a corrupt sprite. SetMixColor pre-
// multiplies sprite color: this halves the alpha while shifting hue.
constexpr std::uint32_t kGhostTint = 0x80FFB0B0u;

using OnDrawHighPrio_t = int(__fastcall*)(void* /*Player*/);
OnDrawHighPrio_t g_original = nullptr;

int __fastcall hooked_OnDrawHighPrio(void* player_this)
{
    // 1) Always draw the local player normally.
    const int result = g_original(player_this);

    // 2) If we have a peer position from the network and we're past
    //    the title screen (peer pos != 0,0), draw the same sprite
    //    again with the AnmVm.pos temporarily swapped to peer coords.
    float peer_x = 0.0f, peer_y = 0.0f;
    std::uint64_t peer_frame = 0;
    if (!th08_platform::net::peek_peer_ghost(peer_x, peer_y, peer_frame)) {
        return result;
    }
    if (peer_x == 0.0f && peer_y == 0.0f) {
        return result;
    }

    auto* p = reinterpret_cast<std::uint8_t*>(player_this);
    auto* anm_pos_x = reinterpret_cast<float*>(p + kOffset_Player_AnmVm_PosX);
    auto* anm_pos_y = reinterpret_cast<float*>(p + kOffset_Player_AnmVm_PosY);

    void* mgr = *reinterpret_cast<void**>(kAddr_g_AnmManager_ptr);

    __try {
        const float saved_x = *anm_pos_x;
        const float saved_y = *anm_pos_y;
        *anm_pos_x = peer_x;
        *anm_pos_y = peer_y;
        if (mgr && g_SetMixColor) {
            g_SetMixColor(mgr, kGhostTint);
        }
        g_original(player_this);
        if (mgr && g_SetMixColorDefault) {
            g_SetMixColorDefault(mgr);
        }
        *anm_pos_x = saved_x;
        *anm_pos_y = saved_y;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Don't permanently disable — just skip this frame's ghost draw.
        // If the SEH fires repeatedly we'd see it in the log; once is fine.
        static bool logged = false;
        if (!logged) {
            logged = true;
            th08_platform::log_line(
                "peer_ghost_sprite: SEH during ghost draw (logged once)");
        }
    }
    return result;
}

void* resolve_target()
{
    HMODULE base = GetModuleHandleA(nullptr);
    if (!base) return nullptr;
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uint8_t*>(base) + kPlayer_OnDrawHighPrio_RVA);
}

}  // namespace

bool install_hook()
{
    void* target = resolve_target();
    if (!target) {
        th08_platform::log_line("peer_ghost_sprite: resolve_target null");
        return false;
    }

    HMODULE base = GetModuleHandleA(nullptr);
    if (base) {
        g_SetMixColor = reinterpret_cast<SetMixColor_t>(
            reinterpret_cast<std::uint8_t*>(base) + kSetMixColor_RVA);
        g_SetMixColorDefault = reinterpret_cast<SetMixColorDefault_t>(
            reinterpret_cast<std::uint8_t*>(base) + kSetMixColorDefault_RVA);
    }
    th08_platform::log_line(
        "phase 6d.3: hooking Player::OnDrawHighPrio at %p (tint=0x%08x)",
        target, kGhostTint);

    if (MH_CreateHook(target, reinterpret_cast<void*>(&hooked_OnDrawHighPrio),
                      reinterpret_cast<LPVOID*>(&g_original)) != MH_OK) {
        th08_platform::log_line("peer_ghost_sprite: MH_CreateHook failed");
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        th08_platform::log_line("peer_ghost_sprite: MH_EnableHook failed");
        return false;
    }
    th08_platform::log_line("phase 6d.3: peer_ghost_sprite enabled");
    return true;
}

void uninstall_hook()
{
    // Hook teardown happens centrally via MH_DisableHook(MH_ALL_HOOKS) in
    // game_loop.cpp's uninstall_game_loop_hook(). Nothing to do here.
}

}  // namespace th08_platform::state::peer_ghost_sprite
