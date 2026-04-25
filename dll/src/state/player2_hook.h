#pragma once

// Sub-phase 5b: auto-construct + auto-register g_Player2 by hooking
// Player::RegisterChain (0x44C230). Whenever ZUN's game-init runs the
// chain registration for g_Player, our detour kicks in immediately
// after and does the same for g_Player2.
//
// MinHook is initialized by install_game_loop_hook(); call this AFTER
// that one so MH_Initialize is already done.
//
// Default behavior: hook is NOT installed unless env
//   TH08_PLATFORM_PLAYER2_AUTO=1
// is set. This keeps the dangerous "two players in the chain" path
// off-by-default until 5c (dual collision) lands.

namespace th08_platform::state {

bool install_player2_hook();
void uninstall_player2_hook();

}  // namespace th08_platform::state
