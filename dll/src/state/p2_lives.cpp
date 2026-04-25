#include "p2_lives.h"

#include <windows.h>
#include <MinHook.h>

#include <atomic>
#include <cstring>

#include "../logging.h"
#include "globals.h"
#include "player2.h"

namespace th08_platform::state::p2_lives {

namespace {

// Verified addresses (codex 5e batch puzzle 5):
//   th08::Player::FUN_0044cbf0 @ 0x44CBF0  __thiscall(Player*) -> int
//     Respawn-init: when player ghost timer expires, this transitions
//     state 3 -> 1, decrements lives via sub_43C641(globals, -1).
//   th08::GameManager::AddLives @ 0x43C641  __thiscall(GameManager*, i32)
//     Writes *(globals+0x74) += (float)a2. Already byte-patched
//     to 94.12% by our Phase 1a patcher.
constexpr DWORD kFun44CBF0_RVA  = 0x44CBF0 - 0x400000;
constexpr DWORD kAddLives_RVA   = 0x43C641 - 0x400000;

using Fun44CBF0_t = int(__fastcall*)(void* this_, void* edx_unused);
using AddLives_t  = void(__fastcall*)(void* gm_this, void* edx_unused, int delta);

Fun44CBF0_t g_orig_fun44CBF0 = nullptr;
AddLives_t  g_orig_addlives  = nullptr;

// State for the cooperating hooks: when fun_0044CBF0 is currently
// executing on g_Player2, the AddLives detour redirects to our counter
// instead of the singleton ZunGlobals field.
std::atomic<bool> g_p2_dying{false};

// P2's own lives counter. Default 3 to match TH08's typical starting
// lives (could be made configurable later).
std::atomic<int> g_p2_lives{3};

std::atomic<unsigned long long> g_redirect_count{0};

int __fastcall hooked_fun44CBF0(void* this_, void* edx)
{
    const bool is_p2 = (this_ == th08_platform::player2::g_Player2);
    if (is_p2) {
        g_p2_dying.store(true, std::memory_order_release);
    }
    const int result = g_orig_fun44CBF0(this_, edx);
    if (is_p2) {
        g_p2_dying.store(false, std::memory_order_release);
    }
    return result;
}

void __fastcall hooked_AddLives(void* gm_this, void* edx, int delta)
{
    if (g_p2_dying.load(std::memory_order_acquire) && delta < 0) {
        // Redirect: P2 took the death, not P1. Don't touch
        // ZunGlobals.livesRemaining at all; bookkeep on our side.
        const int prev = g_p2_lives.fetch_add(delta, std::memory_order_relaxed);
        const unsigned long long count =
            g_redirect_count.fetch_add(1, std::memory_order_relaxed) + 1;
        log_line("p2_lives: redirected death (delta=%d, p2_lives %d -> %d, total redirects=%llu)",
                 delta, prev, prev + delta, count);
        return;
    }
    g_orig_addlives(gm_this, edx, delta);
}

void* resolve(DWORD rva)
{
    HMODULE base = GetModuleHandleA(nullptr);
    if (!base) return nullptr;
    return reinterpret_cast<void*>(reinterpret_cast<std::uint8_t*>(base) + rva);
}

}  // namespace

bool install_hook()
{
    // MH_Initialize is owned by install_game_loop_hook (called first).
    void* t1 = resolve(kFun44CBF0_RVA);
    void* t2 = resolve(kAddLives_RVA);
    if (!t1 || !t2) {
        log_line("p2_lives: resolve failed (t1=%p t2=%p)", t1, t2);
        return false;
    }

    log_line("p2_lives: hooking FUN_0044CBF0 @ %p and AddLives @ %p", t1, t2);

    if (MH_CreateHook(t1, reinterpret_cast<void*>(&hooked_fun44CBF0),
                      reinterpret_cast<LPVOID*>(&g_orig_fun44CBF0)) != MH_OK ||
        MH_CreateHook(t2, reinterpret_cast<void*>(&hooked_AddLives),
                      reinterpret_cast<LPVOID*>(&g_orig_addlives)) != MH_OK) {
        log_line("p2_lives: MH_CreateHook failed");
        return false;
    }
    if (MH_EnableHook(t1) != MH_OK || MH_EnableHook(t2) != MH_OK) {
        log_line("p2_lives: MH_EnableHook failed");
        return false;
    }
    log_line("p2_lives: enabled (P2 starts with %d lives)",
             g_p2_lives.load(std::memory_order_relaxed));
    return true;
}

void uninstall_hook()
{
    void* t1 = resolve(kFun44CBF0_RVA);
    void* t2 = resolve(kAddLives_RVA);
    if (g_orig_fun44CBF0 && t1) { MH_DisableHook(t1); MH_RemoveHook(t1); }
    if (g_orig_addlives  && t2) { MH_DisableHook(t2); MH_RemoveHook(t2); }
    g_orig_fun44CBF0 = nullptr;
    g_orig_addlives = nullptr;
}

int snapshot_p2_lives() noexcept
{
    return g_p2_lives.load(std::memory_order_relaxed);
}

unsigned long long snapshot_redirect_count() noexcept
{
    return g_redirect_count.load(std::memory_order_relaxed);
}

}  // namespace th08_platform::state::p2_lives
