// th08_platform.dll entry point.
// Phase 2 installs the frame hook plus Controller::GetInput observation.

#include <windows.h>

#include <cstdint>
#include <cstdlib>

#include "logging.h"
#include "hooks/game_loop.h"
#include "hooks/input.h"
#include "hooks/instance_unmutex.h"
#include "net/lockstep.h"
#include "net/pump_thread.h"
#include "net/rollback.h"
#include "state/peer_ghost.h"
#include "state/player2.h"
#include "state/player2_hook.h"
#include "state/dual_collision.h"
#include "state/enemy_aim.h"
#include "state/item_routing.h"
#include "state/p2_input.h"
#include "state/p2_lives.h"
#include "state/hud.h"
#include "state/p2_mirror.h"

namespace {

// Phase 5 (multiplayer) default-on policy: phase 5 sub-features auto-enable
// without env vars so the loader-launched DLL "just works" for 2P play.
// Set TH08_PLATFORM_DISABLE_MULTIPLAYER=1 to turn off the entire phase-5
// stack at once. Set an individual sub-flag (e.g. TH08_PLATFORM_HUD=0) to
// disable just that one. Phase 4 networking flags (PEER / LISTEN /
// FAKE_RTT_MS) remain opt-in because they require explicit configuration.
bool env_flag(const char* name, bool default_value)
{
    char buf[8] = {};
    const DWORD len = GetEnvironmentVariableA(name, buf,
                                              static_cast<DWORD>(sizeof(buf)));
    if (len == 0) return default_value;
    // Treat "0" / empty as disable, anything else as enable.
    return buf[0] != '0';
}

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
    th08_platform::log_line("phase 6a: ZUN single-instance check patch %s",
                            th08_platform::hooks::was_zun_single_instance_check_patched()
                                ? "applied" : "FAILED");
    th08_platform::net::set_fake_rtt_ms(read_fake_rtt_ms());
    th08_platform::net::rollback::reset();

    char peer_spec[64] = {};
    const DWORD peer_len = GetEnvironmentVariableA("TH08_PLATFORM_PEER", peer_spec,
                                                   static_cast<DWORD>(sizeof(peer_spec)));
    const bool host_mode = GetEnvironmentVariableA("TH08_PLATFORM_HOST", nullptr, 0) > 0;
    const std::uint16_t listen_port = read_listen_port();
    const bool net_ok =
        peer_len != 0 ? th08_platform::net::initialize(peer_spec, listen_port)
                      : (!host_mode ||
                         th08_platform::net::initialize_listen_only(listen_port));

    const bool game_loop_ok = th08_platform::hooks::install_game_loop_hook();
    const bool input_ok = game_loop_ok && th08_platform::hooks::install_input_hook();
    const bool rollback_audio_ok =
        input_ok && th08_platform::net::rollback::install_audio_hooks();

    if ((peer_len != 0 || host_mode) && !net_ok) {
        th08_platform::log_line("phase 4 net init failed; staying local-only");
    }

    // Phase 6b: drive net::poll() outside of the GetInput / OnUpdate
    // chain so packets pump even at title screen (where GameManager::
    // OnUpdate doesn't fire).
    if (net_ok && (peer_len != 0 || host_mode)) {
        th08_platform::net::start_pump_thread();
        // Phase 6d.2: arm AsciiManager-based peer label render. No-ops
        // until the first Ctrl_Ghost packet lands.
        th08_platform::state::peer_ghost::install();
    }

    // Phase 5 (multiplayer) is default-on. Set TH08_PLATFORM_DISABLE_MULTIPLAYER=1
    // to turn off the entire phase-5 stack, or set a specific sub-flag to 0
    // to disable just that one (e.g. TH08_PLATFORM_HUD=0).
    const bool multiplayer = !env_flag("TH08_PLATFORM_DISABLE_MULTIPLAYER", false);
    if (!multiplayer) {
        th08_platform::log_line("phase 5: TH08_PLATFORM_DISABLE_MULTIPLAYER=1, multiplayer stack disabled");
    }

    // Sub-phase 5b: hook Player::RegisterChain so g_Player2 is auto-built
    // when ZUN's Player::RegisterChain runs.
    if (multiplayer && env_flag("TH08_PLATFORM_PLAYER2_AUTO", true)) {
        th08_platform::log_line("phase 5b: installing Player::RegisterChain hook");
        const bool hook_ok = th08_platform::state::install_player2_hook();
        th08_platform::log_line("phase 5b: hook install %s",
                                hook_ok ? "ok" : "FAILED");
    }

    // Sub-phase 5a (manual / one-shot, distinct from 5b's auto-hook).
    // Off by default; only enable for exploratory testing.
    //   TH08_PLATFORM_TEST_PLAYER2=1 -> Construct() only (alloc + ctor)
    //   TH08_PLATFORM_TEST_PLAYER2=2 -> Construct() + Register() (chain)
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

    // Sub-phase 5i: dual bullet/laser routing.
    if (multiplayer && env_flag("TH08_PLATFORM_DUAL_COLLISION", true)) {
        th08_platform::log_line("phase 5i: installing dual_collision v2 hooks");
        const bool dc_ok = th08_platform::state::dual_collision::install_hook();
        th08_platform::log_line("phase 5i: dual_collision v2 install %s",
                                dc_ok ? "ok" : "FAILED");
    }

    // Sub-phase 5i Batch B: item collection/attraction routing.
    if (multiplayer && env_flag("TH08_PLATFORM_ITEM_ROUTING", true)) {
        th08_platform::log_line("phase 5i: installing item_routing hooks");
        const bool item_ok = th08_platform::state::item_routing::install();
        th08_platform::log_line("phase 5i: item_routing install %s",
                                item_ok ? "ok" : "FAILED");
    }

    // Sub-phase 5i Batch C/D: enemy aim routing.
    //
    // DEFAULT OFF as of 2026-04-26 after runtime testing exposed two design
    // flaws:
    //   (1) sub_428310 ("VEC" hook) is a generic vector helper called
    //       1500+ times per second by background code even when no enemies
    //       are firing. Each call entered our spoof path (write
    //       g_Player.pos.x/y, call original, restore), pushing the frame
    //       budget past 16.67ms and dropping the game from 60fps to 24fps.
    //   (2) The 0x426C40 table-dispatched block (TBL), the EnemyManager
    //       whole-function spoof (ENM), and the Spellcard whole-function
    //       spoof (SPC) have ABI risk (non-function-aligned hook target,
    //       non-local exits inside ~6KB functions). With all 10 hooks live
    //       the game froze during enemy spawn.
    //
    // The enemy_aim code is kept (env opt-in) but won't auto-install.
    // Future redesign: VEC must use return-address gating like
    // item_routing's sub_44C1B0 (only route when caller is an enemy
    // firing site, pass through otherwise). ENM/SPC may need per-branch
    // hooks instead of whole-function spoof.
    //
    // Opt-in for testing: TH08_PLATFORM_ENEMY_AIM=1, optionally narrow
    // per hook via TH08_PLATFORM_AIM_<NAME>=0/1.
    if (multiplayer && env_flag("TH08_PLATFORM_ENEMY_AIM", false)) {
        th08_platform::log_line("phase 5i: installing enemy_aim hooks (env opt-in)");
        const bool aim_ok = th08_platform::state::enemy_aim::install();
        th08_platform::log_line("phase 5i: enemy_aim install %s",
                                aim_ok ? "ok" : "FAILED");
    }

    // Sub-phase 5d: per-player input routing.
    if (multiplayer && env_flag("TH08_PLATFORM_P2_INPUT", true)) {
        th08_platform::log_line("phase 5d: installing OnUpdate input-swap hook");
        const bool p2i_ok = th08_platform::state::p2_input::install_hook();
        th08_platform::log_line("phase 5d: p2_input install %s",
                                p2i_ok ? "ok" : "FAILED");
    }

    // Sub-phase 5f: per-player lives routing.
    if (multiplayer && env_flag("TH08_PLATFORM_P2_LIVES", true)) {
        th08_platform::log_line("phase 5f: installing FUN_44CBF0+AddLives hooks");
        const bool p2l_ok = th08_platform::state::p2_lives::install_hook();
        th08_platform::log_line("phase 5f: p2_lives install %s",
                                p2l_ok ? "ok" : "FAILED");
    }

    // Sub-phase 5g: visual HUD for P2 via AsciiManager.
    if (multiplayer && env_flag("TH08_PLATFORM_HUD", true)) {
        th08_platform::log_line("phase 5g: installing AsciiManager-based P2 HUD");
        const bool hud_ok = th08_platform::state::hud::install();
        th08_platform::log_line("phase 5g: hud install %s",
                                hud_ok ? "ok" : "FAILED");
    }

    // Sub-phase 5h: mirror hardcoded-g_Player call sites for g_Player2.
    if (multiplayer && env_flag("TH08_PLATFORM_P2_MIRROR", true)) {
        th08_platform::log_line("phase 5h: installing 3 mirror hooks");
        const bool mirror_ok = th08_platform::state::p2_mirror::install();
        th08_platform::log_line("phase 5h: p2_mirror install %s",
                                mirror_ok ? "ok" : "FAILED");
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
        // Phase 6a: must run BEFORE main thread resumes so th08's
        // sub_443420 takes the patched (always-success) branch. The
        // patch is one byte + VirtualProtect — safe in DllMain.
        th08_platform::hooks::patch_zun_single_instance_check();
        // Avoid other non-trivial work in DllMain itself (loader lock).
        CreateThread(nullptr, 0, dll_init_thread, nullptr, 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        th08_platform::state::p2_mirror::uninstall();
        th08_platform::state::hud::uninstall();
        th08_platform::state::p2_lives::uninstall_hook();
        th08_platform::state::p2_input::uninstall_hook();
        th08_platform::state::enemy_aim::uninstall();
        th08_platform::state::item_routing::uninstall();
        th08_platform::state::dual_collision::uninstall_hook();
        th08_platform::state::uninstall_player2_hook();
        th08_platform::hooks::uninstall_input_hook();
        th08_platform::hooks::uninstall_game_loop_hook();
        // Stop pump BEFORE net::shutdown so the thread isn't poking the
        // socket while we close it.
        th08_platform::state::peer_ghost::uninstall();
        th08_platform::net::stop_pump_thread();
        th08_platform::net::shutdown();
        th08_platform::log_shutdown();
        break;
    }
    return TRUE;
}
