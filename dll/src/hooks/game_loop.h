#pragma once
// Phase 0 hook: th08::GameManager::OnUpdate @ 0x43aa03
// Purpose: prove injection + MinHook plumbing work; tick a frame counter.

namespace th08_platform::hooks {

bool install_game_loop_hook();
void uninstall_game_loop_hook();

}  // namespace th08_platform::hooks
