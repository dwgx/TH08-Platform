#include "game_loop.h"
#include "../logging.h"
#include "../net/lockstep.h"
#include "../net/rollback.h"
#include "../state/hud.h"
#include "../state/peer_ghost.h"
#include "../state/player2_hook.h"
#include "../state/rng_sync.h"

#include <windows.h>
#include <MinHook.h>
#include <atomic>
#include <cstdint>

namespace th08_platform::hooks {

namespace {
// th08::GameManager::OnUpdate in th08.exe v1.00d.
// From config/mapping.csv (line 528):
//   th08::GameManager::OnUpdate,0x439bc7,0xe3c,unknown,,u8
// Mapping marks it `unknown`, but the decomp's ChainCallback pattern
// (AsciiManager/Supervisor/Gui/etc.) is __fastcall — gm in ECX, no stack
// args. Image base is 0x400000.
//
// (0x43aa03 is GameManager::OnDraw, NOT OnUpdate — earlier scaffold
// used the wrong address and crashed entering a stage.)
constexpr DWORD kGameManagerOnUpdateRVA = 0x439bc7 - 0x400000;

using OnUpdate_t = int(__fastcall*)(void* /*GameManager*/);
OnUpdate_t g_original_OnUpdate = nullptr;

std::atomic<uint64_t> g_frame_count{0};

int __fastcall hooked_OnUpdate(void* gm)
{
    // Phase 6f.2: stage-entry gate. Announces local stage entry on the
    // first hooked OnUpdate tick, then while the peer hasn't reported
    // in (Ctrl_Start_Game not yet received) we (a) paint the
    // "WAITING FOR P1/P2..." overlay and (b) write isInGameMenu = 1 on
    // GameManager so TH08 treats the frame as paused (same flag the ESC
    // pause menu uses; GameManager.hpp:275, GameManager.cpp:54-72 reads
    // it from OnDraw). When peer arrives we clear the flag once and
    // unlock; further frames pass through unmodified.
    //
    // Skipping the original OnUpdate (early return with chain CONTINUE)
    // crashed the game in ~2s -- the chain executor expects each
    // GameManager tick to actually run. The isInGameMenu flag-flip is
    // the supported pause path that ZUN already uses, so it should be
    // safe.
    static std::atomic<bool> s_start_announced{false};
    static std::atomic<bool> s_unlocked{false};
    constexpr std::uintptr_t kAddr_GameManager_isInGameMenu = 0x0160F508 + 0x3DBB2;
    auto* const isInGameMenu =
        reinterpret_cast<volatile std::uint8_t*>(kAddr_GameManager_isInGameMenu);

    if (th08_platform::net::is_configured() &&
        !s_start_announced.exchange(true, std::memory_order_acq_rel)) {
        th08_platform::net::send_start_game(1);
    }
    const bool waiting =
        th08_platform::net::is_configured() &&
        (!th08_platform::net::is_connected() ||
         th08_platform::net::peer_start_game_frame() < 0);
    if (waiting) {
        th08_platform::state::peer_ghost::enqueue_waiting_overlay();
        // Force-pause via the same flag ESC sets. SEH-wrap the write
        // because GameManager+0x3DBB2 is only valid after the GM ctor
        // ran, which it has by the time OnUpdate first ticks.
        __try {
            *isInGameMenu = 1;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            static bool logged = false;
            if (!logged) {
                logged = true;
                th08_platform::log_line(
                    "phase 6f.2: SEH writing isInGameMenu (logged once)");
            }
        }
    } else if (!s_unlocked.exchange(true, std::memory_order_acq_rel)) {
        // First non-waiting tick — clear the pause flag so the stage
        // can run. Subsequent ticks let ZUN's own ESC handler manage
        // it without our interference.
        __try {
            *isInGameMenu = 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        th08_platform::log_line(
            "phase 6f.2: stage entry gate released (peer ready)");
    }

    const auto f = g_frame_count.fetch_add(1, std::memory_order_relaxed) + 1;
    // Seed before the stage script's first RNG read in the original update.
    if (f == 1) {
        th08_platform::state::rng_sync::apply_if_ready();
    }
    th08_platform::net::rollback::on_frame_start(f);
    th08_platform::state::on_frame_tick(static_cast<unsigned long long>(f));
    // Phase 5g: queue P2 stats into AsciiManager BEFORE original
    // GameManager::OnUpdate runs so they're rendered by AsciiManager's
    // OnDrawHighPrio later in the same frame's draw chain.
    th08_platform::state::hud::enqueue_p2_strings();
    th08_platform::state::peer_ghost::enqueue_peer_label();
    if (f % 60 == 0) {
        th08_platform::log_line("GameManager::OnUpdate tick: frame %llu",
                                static_cast<unsigned long long>(f));
    }

    const int result = g_original_OnUpdate(gm);
    th08_platform::net::rollback::on_frame_end(gm, f);
    return result;
}

void* resolve_target()
{
    HMODULE base = GetModuleHandleA(nullptr);
    if (!base) return nullptr;
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uint8_t*>(base) + kGameManagerOnUpdateRVA);
}
}  // namespace

bool install_game_loop_hook()
{
    if (MH_Initialize() != MH_OK) {
        th08_platform::log_line("MH_Initialize failed");
        return false;
    }
    void* target = resolve_target();
    if (!target) {
        th08_platform::log_line("resolve_target returned null");
        return false;
    }
    th08_platform::log_line("hooking GameManager::OnUpdate at %p", target);

    if (MH_CreateHook(target, reinterpret_cast<void*>(&hooked_OnUpdate),
                      reinterpret_cast<LPVOID*>(&g_original_OnUpdate)) != MH_OK) {
        th08_platform::log_line("MH_CreateHook failed");
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        th08_platform::log_line("MH_EnableHook failed");
        return false;
    }
    th08_platform::log_line("GameManager::OnUpdate hook enabled");
    return true;
}

void uninstall_game_loop_hook()
{
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}

std::uint64_t current_frame()
{
    return g_frame_count.load(std::memory_order_relaxed);
}

int run_original_update(void* game_manager)
{
    return g_original_OnUpdate(game_manager);
}

}  // namespace th08_platform::hooks
