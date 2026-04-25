#pragma once

#include <cstdint>

namespace th08_platform::hooks {

bool install_game_loop_hook();
void uninstall_game_loop_hook();
std::uint64_t current_frame();

}  // namespace th08_platform::hooks
