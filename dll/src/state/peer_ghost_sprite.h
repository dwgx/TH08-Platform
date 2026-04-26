#pragma once

namespace th08_platform::state::peer_ghost_sprite {

// Phase 6d.3: replace the floating "P1"/"P2" label with an actual
// player sprite at peer's world coords. Hooks Player::OnDrawHighPrio
// (0x44D530) and runs the original draw twice — once with the local
// player's AnmVm.pos (the standard frame), once after writing the
// peer's world pos into the same AnmVm.pos so the draw lands at the
// peer's position. AnmVm.pos sits at Player+0x10+0x208 = Player+0x218.
//
// Caveats:
//   - Both draws use the LOCAL player's animation state. Peer ghost
//     looks like local player's character, NOT peer's actual choice.
//     Acceptable for now since we don't transmit shotType yet.
//   - Calling OnDrawHighPrio twice doubles vertex-buffer pressure and
//     any per-frame state writes inside the original. Tested OK in
//     practice; SEH-wrapped just in case.
//   - The text label from peer_ghost.cpp is kept as a redundant
//     identifier so users can tell ghost from local at a glance.

bool install_hook();
void uninstall_hook();

}  // namespace th08_platform::state::peer_ghost_sprite
