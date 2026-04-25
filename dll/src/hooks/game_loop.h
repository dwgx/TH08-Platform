#pragma once

#include <cstdint>

namespace th08_platform::hooks {

bool install_game_loop_hook();
void uninstall_game_loop_hook();
std::uint64_t current_frame();
int run_original_update(void* game_manager);

}  // namespace th08_platform::hooks
