// th08_platform.dll entry point.
// Phase 2 installs the frame hook plus Controller::GetInput observation.

#include <windows.h>

#include <cstdint>
#include <cstdlib>

#include "logging.h"
#include "hooks/game_loop.h"
#include "hooks/input.h"
#include "net/lockstep.h"
#include "net/rollback.h"
#include "state/player2.h"
#include "state/player2_hook.h"
#include "state/dual_collision.h"

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

std::uint32_t read_fake_rtt_ms()
{
    char value[32] = {};
    if (GetEnvironmentVariableA("TH08_PLATFORM_FAKE_RTT_MS", value,
                                static_cast<DWORD>(sizeof(value))) == 0) {
        return 0;
    }

    char* end = nullptr;
    const unsigned long delay = std::strtoul(value, &end, 10);
    if (!end || *end != '\0') {
        th08_platform::log_line("invalid TH08_PLATFORM_FAKE_RTT_MS='%s', defaulting to 0",
                                value);
        return 0;
    }

    return static_cast<std::uint32_t>(delay);
}

DWORD WINAPI dll_init_thread(LPVOID)
{
    th08_platform::log_init();
    th08_platform::log_line("th08_platform.dll attached; starting hooks");
    th08_platform::net::set_fake_rtt_ms(read_fake_rtt_ms());
    th08_platform::net::rollback::reset();

    char peer_spec[64] = {};
    const DWORD peer_len = GetEnvironmentVariableA("TH08_PLATFORM_PEER", peer_spec,
                                                   static_cast<DWORD>(sizeof(peer_spec)));
    const bool net_ok =
        peer_len == 0 || th08_platform::net::initialize(peer_spec, read_listen_port());

    const bool game_loop_ok = th08_platform::hooks::install_game_loop_hook();
    const bool input_ok = game_loop_ok && th08_platform::hooks::install_input_hook();
    const bool rollback_audio_ok =
        input_ok && th08_platform::net::rollback::install_audio_hooks();

    if (peer_len != 0 && !net_ok) {
        th08_platform::log_line("phase 4 net init failed; staying local-only");
    }

    // Sub-phase 5b opt-in: TH08_PLATFORM_PLAYER2_AUTO=1 hooks
    // Player::RegisterChain (0x44C230); on the first invocation, our
    // detour runs the original then auto-Construct() + Register()
    // g_Player2. This is the auto-flow that 5c+ will build on.
    char p2_auto_env[8] = {};
    if (GetEnvironmentVariableA("TH08_PLATFORM_PLAYER2_AUTO", p2_auto_env,
                                static_cast<DWORD>(sizeof(p2_auto_env))) > 0 &&
        p2_auto_env[0] == '1') {
        th08_platform::log_line("phase 5b: TH08_PLATFORM_PLAYER2_AUTO=1, installing Player::RegisterChain hook");
        const bool hook_ok = th08_platform::state::install_player2_hook();
        th08_platform::log_line("phase 5b: hook install %s",
                                hook_ok ? "ok" : "FAILED");
    }

    // Sub-phase 5a opt-in (manual / one-shot, distinct from 5b's auto-hook):
    //   TH08_PLATFORM_TEST_PLAYER2=1 -> Construct() only (alloc + ctor)
    //   TH08_PLATFORM_TEST_PLAYER2=2 -> Construct() + Register() (chain)
    //
    // The =2 path is HAZARDOUS: chain dispatcher will tick g_Player2's
    // OnUpdate every frame, but AddedCallback is not yet driven so
    // helper FUN_'s may dereference NULL fields. Use only for
    // exploratory crash testing.
    //
    // Both paths are dangerous if the game hasn't finished initializing
    // AnmManager/BulletManager - prefer to set ONLY after manually
    // playing past the title screen, then re-attaching.
    char p2_env[8] = {};
    if (GetEnvironmentVariableA("TH08_PLATFORM_TEST_PLAYER2", p2_env,
                                static_cast<DWORD>(sizeof(p2_env))) > 0 &&
        (p2_env[0] == '1' || p2_env[0] == '2')) {
        th08_platform::log_line("phase 5a: TH08_PLATFORM_TEST_PLAYER2=%c, attempting g_Player2 ctor",
                                p2_env[0]);
        const bool p2_ok = th08_platform::player2::Construct();
        th08_platform::log_line("phase 5a: g_Player2 construct %s",
                                p2_ok ? "ok" : "FAILED");
        if (p2_ok && p2_env[0] == '2') {
            th08_platform::log_line("phase 5a: TH08_PLATFORM_TEST_PLAYER2=2, attempting g_Player2 chain register");
            const bool reg_ok = th08_platform::player2::Register();
            th08_platform::log_line("phase 5a: g_Player2 register %s",
                                    reg_ok ? "ok" : "FAILED");
        }
    }

    // Sub-phase 5c opt-in: dual collision (Player::TestHitbox hook).
    // Independent of 5b's auto-hook; useful only with PLAYER2_AUTO=1
    // since the dual test no-ops if g_Player2 isn't constructed.
    char dc_env[8] = {};
    if (GetEnvironmentVariableA("TH08_PLATFORM_DUAL_COLLISION", dc_env,
                                static_cast<DWORD>(sizeof(dc_env))) > 0 &&
        dc_env[0] == '1') {
        th08_platform::log_line("phase 5c: TH08_PLATFORM_DUAL_COLLISION=1, installing TestHitbox hook");
        const bool dc_ok = th08_platform::state::dual_collision::install_hook();
        th08_platform::log_line("phase 5c: dual_collision install %s",
                                dc_ok ? "ok" : "FAILED");
    }

    if (game_loop_ok && input_ok && rollback_audio_ok && net_ok) {
        th08_platform::log_line("phase 4 ready");
    } else if (!game_loop_ok) {
        th08_platform::log_line("phase 4 game loop hook install failed");
    } else if (!input_ok) {
        th08_platform::log_line("phase 4 input hook install failed");
    } else if (!rollback_audio_ok) {
        th08_platform::log_line("phase 4 audio hook install failed");
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
        th08_platform::state::dual_collision::uninstall_hook();
        th08_platform::state::uninstall_player2_hook();
        th08_platform::hooks::uninstall_input_hook();
        th08_platform::hooks::uninstall_game_loop_hook();
        th08_platform::net::shutdown();
        th08_platform::log_shutdown();
        break;
    }
    return TRUE;
}
