#include "input.h"

#include "../logging.h"
#include "../net/lockstep.h"
#include "../state/globals.h"
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
std::uint64_t g_last_synced_frame = ~0ull;
std::uint16_t g_last_logged_input = 0xffff;

int __fastcall hooked_GetInput()
{
    const int ret = g_original_GetInput();

    th08_platform::net::poll();

    const auto frame = th08_platform::hooks::current_frame();
    const auto cur = *globals::at<std::uint16_t>(globals::kAddr_g_CurFrameInput);
    const auto held = *globals::at<std::uint32_t>(globals::kAddr_g_NumOfFramesInputsWereHeld);
    const auto n = g_input_call_count.fetch_add(1, std::memory_order_relaxed) + 1;

    if (th08_platform::net::is_connected() && frame != g_last_synced_frame) {
        th08_platform::net::send_local_input(frame, cur);

        const auto peer_input =
            th08_platform::net::wait_for_peer_input(frame, /*timeout_ms=*/200);
        if (peer_input.has_value()) {
            th08_platform::log_line(
                "synced frame %llu: local=0x%04x remote=0x%04x",
                static_cast<unsigned long long>(frame), cur, *peer_input);
        } else {
            th08_platform::log_line("peer timeout at frame %llu (local=0x%04x)",
                                    static_cast<unsigned long long>(frame), cur);
        }
        g_last_synced_frame = frame;
    }

    if ((n % 60u) == 0 || cur != g_last_logged_input) {
        th08_platform::log_line("input: cur=0x%04x (held=%u)", cur, held);
        g_last_logged_input = cur;
    }

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
