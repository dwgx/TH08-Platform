// th08_platform.dll — Phase 0 entry point.
// Minimal proof of the plumbing: installs a hook on GameManager::OnUpdate
// and writes frame ticks to the log file. Everything heavier (netcode,
// state capture, P2) lands in subsequent phases per docs/DLL_PLAN.md.

#include <windows.h>

#include "logging.h"
#include "hooks/game_loop.h"

namespace {
DWORD WINAPI dll_init_thread(LPVOID)
{
    th08_platform::log_init();
    th08_platform::log_line("th08_platform.dll attached; starting phase 0 hooks");
    if (th08_platform::hooks::install_game_loop_hook()) {
        th08_platform::log_line("phase 0 ready");
    } else {
        th08_platform::log_line("phase 0 hook install failed");
    }
    return 0;
}
}  // namespace

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID /*reserved*/)
{
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hinst);
        // Avoid non-trivial work in DllMain itself (loader lock).
        CreateThread(nullptr, 0, dll_init_thread, nullptr, 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        th08_platform::hooks::uninstall_game_loop_hook();
        th08_platform::log_shutdown();
        break;
    }
    return TRUE;
}
