#include "game_loop.h"
#include "../logging.h"

#include <windows.h>
#include <MinHook.h>
#include <atomic>
#include <cstdint>

namespace th08_platform::hooks {

namespace {
// th08::GameManager::OnUpdate in th08.exe v1.00d.
// From game/config/mapping.csv:
//   th08::GameManager::OnUpdate,0x43aa03,0x59,__cdecl,,i32,GameManager*
// Signature: ChainCallbackResult __cdecl OnUpdate(GameManager *gm);
// th08's image base is 0x400000 — file-offset subtraction gives the RVA.
constexpr DWORD kGameManagerOnUpdateRVA = 0x43aa03 - 0x400000;

using OnUpdate_t = int(__cdecl*)(void* /*GameManager*/);
OnUpdate_t g_original_OnUpdate = nullptr;

std::atomic<uint64_t> g_frame_count{0};

int __cdecl hooked_OnUpdate(void* gm)
{
    const auto f = g_frame_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (f % 60 == 0) {
        th08_platform::log_line("GameManager::OnUpdate tick: frame %llu",
                                static_cast<unsigned long long>(f));
    }
    return g_original_OnUpdate(gm);
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

}  // namespace th08_platform::hooks
