#include "input.h"

#include "../logging.h"
#include "../state/globals.h"

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

int __fastcall hooked_GetInput()
{
    const int ret = g_original_GetInput();

    const auto cur = *globals::at<std::uint16_t>(globals::kAddr_g_CurFrameInput);
    const auto last = *globals::at<std::uint16_t>(globals::kAddr_g_LastFrameInput);
    const auto held = *globals::at<std::uint32_t>(globals::kAddr_g_NumOfFramesInputsWereHeld);
    const auto n = g_input_call_count.fetch_add(1, std::memory_order_relaxed) + 1;

    if ((n % 60u) == 0 || cur != last) {
        th08_platform::log_line("input: cur=0x%04x (held=%u)", cur, held);
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
