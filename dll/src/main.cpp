// th08_platform.dll entry point.
// Phase 2 installs the frame hook plus Controller::GetInput observation.

#include <windows.h>

#include <cstdint>
#include <cstdlib>

#include "logging.h"
#include "hooks/game_loop.h"
#include "hooks/input.h"
#include "net/lockstep.h"

namespace {
std::uint16_t read_listen_port()
{
    char value[32] = {};
    if (GetEnvironmentVariableA("TH08_PLATFORM_LISTEN", value,
                                static_cast<DWORD>(sizeof(value))) == 0) {
        return 7480;
    }

    char* end = nullptr;
    const unsigned long port = std::strtoul(value, &end, 10);
    if (!end || *end != '\0' || port == 0 || port > 65535) {
        th08_platform::log_line("invalid TH08_PLATFORM_LISTEN='%s', defaulting to 7480",
                                value);
        return 7480;
    }

    return static_cast<std::uint16_t>(port);
}

DWORD WINAPI dll_init_thread(LPVOID)
{
    th08_platform::log_init();
    th08_platform::log_line("th08_platform.dll attached; starting hooks");

    char peer_spec[64] = {};
    const DWORD peer_len = GetEnvironmentVariableA("TH08_PLATFORM_PEER", peer_spec,
                                                   static_cast<DWORD>(sizeof(peer_spec)));
    const bool net_ok =
        peer_len == 0 || th08_platform::net::initialize(peer_spec, read_listen_port());

    const bool game_loop_ok = th08_platform::hooks::install_game_loop_hook();
    const bool input_ok = game_loop_ok && th08_platform::hooks::install_input_hook();

    if (peer_len != 0 && !net_ok) {
        th08_platform::log_line("phase 3 lockstep init failed; staying observation-only");
    }

    if (game_loop_ok && input_ok && net_ok) {
        th08_platform::log_line("phase 3 ready");
    } else if (!game_loop_ok) {
        th08_platform::log_line("phase 3 game loop hook install failed");
    } else if (!input_ok) {
        th08_platform::log_line("phase 3 input hook install failed");
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
        th08_platform::hooks::uninstall_input_hook();
        th08_platform::hooks::uninstall_game_loop_hook();
        th08_platform::net::shutdown();
        th08_platform::log_shutdown();
        break;
    }
    return TRUE;
}
