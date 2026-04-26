#include "input.h"

#include "../logging.h"
#include "../net/lockstep.h"
#include "../state/globals.h"
#include "../state/peer_ghost.h"
#include "game_loop.h"

#include <windows.h>
#include <MinHook.h>

#include <atomic>
#include <cstdint>

namespace th08_platform::hooks {

namespace {
constexpr DWORD kControllerGetInputRVA = 0x43d970 - 0x400000;

// Hex-Rays for 0x43d970 exports `int()` with no arguments. We keep the
// project's Controller hook convention as __fastcall; with zero arguments,
// the call boundary is equivalent and the hook remains non-invasive.
using GetInput_t = int(__fastcall*)();
GetInput_t g_original_GetInput = nullptr;

std::atomic<std::uint64_t> g_input_call_count{0};
std::uint16_t g_last_logged_input = 0xffff;

int __fastcall hooked_GetInput()
{
    const int ret = g_original_GetInput();

    const auto frame = th08_platform::hooks::current_frame();
    const auto cur = *globals::at<std::uint16_t>(globals::kAddr_g_CurFrameInput);
    const auto held = *globals::at<std::uint32_t>(globals::kAddr_g_NumOfFramesInputsWereHeld);
    const auto n = g_input_call_count.fetch_add(1, std::memory_order_relaxed) + 1;

    if ((n % 60u) == 0 || cur != g_last_logged_input) {
        th08_platform::log_line("input: cur=0x%04x (held=%u)", cur, held);
        g_last_logged_input = cur;
    }

    // Phase 6b: feed local input into the lockstep state machine, and
    // every kKeyPackFrameNum frames flush a Pack to the peer. `frame`
    // only ticks once GameManager::OnUpdate is firing (i.e., post-stage-
    // select); on title-screen these calls early-out via
    // is_configured() / is_connected() inside lockstep.
    th08_platform::net::capture_local_input(frame, cur);
    th08_platform::net::send_input_pack_if_due(frame);
    // Phase 6d.1: stream local player position so the peer can render us
    // as a ghost. g_Player is at a fixed image-base address; pos.x/y at
    // +0x2B4/+0x2B8 are the live world coords (not the static spawn
    // coords at +0x3D4/+0x3D8).
    auto* gp = reinterpret_cast<const std::uint8_t*>(0x017D5EF8);
    const float px = *reinterpret_cast<const float*>(gp + 0x2B4);
    const float py = *reinterpret_cast<const float*>(gp + 0x2B8);
    // Phase 6e.2/6e.3: forward stats via GameManager's stats sub-struct
    // at *(GM+8). Offsets verified via mapping.csv + IDA decomp:
    //   AddLives    @ 0x43C641 → +0x74 float
    //   SetBombCount@ 0x406F30 → +0x80 float
    //   SetPower    @ 0x406FA0 → +0x98 float
    //   AddScore    @ 0x4181F0 → +0x08 i32 (score / 10 in ZUN's units)
    // SEH-wrapped because the +8 deref is null pre-stage.
    std::uint16_t lives = 0, bombs = 0, power = 0;
    std::uint32_t score = 0;
    __try {
        auto* gm = reinterpret_cast<const std::uint8_t*>(0x0160F508);
        auto* stats = *reinterpret_cast<const std::uint8_t* const*>(gm + 8);
        if (stats) {
            const float l = *reinterpret_cast<const float*>(stats + 0x74);
            const float b = *reinterpret_cast<const float*>(stats + 0x80);
            const float p = *reinterpret_cast<const float*>(stats + 0x98);
            const std::int32_t s = *reinterpret_cast<const std::int32_t*>(stats + 0x08);
            // Tight clamps to suppress pre-stage noise. TH08 vanilla
            // maxes are 9 lives, 9 bombs, 128 power; anything wider is
            // garbage memory (stats struct allocated but un-init).
            if (l >= 0.0f && l <  64.0f) lives = static_cast<std::uint16_t>(l);
            if (b >= 0.0f && b <  64.0f) bombs = static_cast<std::uint16_t>(b);
            if (p >= 0.0f && p < 256.0f) power = static_cast<std::uint16_t>(p);
            if (s >= 0 && s < 0x40000000) score = static_cast<std::uint32_t>(s);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // safe defaults already in scope
    }
    th08_platform::net::send_ghost_pack(frame, px, py, lives, bombs, power, score);

    // Phase 6e.4: queue the always-on connection/HUD strip from here so
    // it renders at the title screen too (GetInput ticks on the title
    // menu; OnUpdate does not).
    th08_platform::state::peer_ghost::enqueue_status_strip();

    return ret;
}

void* resolve_target()
{
    HMODULE base = GetModuleHandleA(nullptr);
    if (!base) return nullptr;
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uint8_t*>(base) + kControllerGetInputRVA);
}
}  // namespace

bool install_input_hook()
{
    void* target = resolve_target();
    if (!target) {
        th08_platform::log_line("input hook: resolve_target null");
        return false;
    }
    th08_platform::log_line("hooking Controller::GetInput at %p", target);

    if (MH_CreateHook(target, reinterpret_cast<void*>(&hooked_GetInput),
                      reinterpret_cast<LPVOID*>(&g_original_GetInput)) != MH_OK) {
        th08_platform::log_line("input hook: MH_CreateHook failed");
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        th08_platform::log_line("input hook: MH_EnableHook failed");
        return false;
    }

    th08_platform::log_line("Controller::GetInput hook enabled");
    return true;
}

void uninstall_input_hook()
{
    // Hook shutdown is centralized through MH_DisableHook(MH_ALL_HOOKS).
}

}  // namespace th08_platform::hooks
