#pragma once

namespace th08_platform::state::peer_ghost {

// Resolve AsciiManager::AddString and arm the ghost-render path. Called
// from dll_init_thread once. Safe to call without a connected peer —
// enqueue_peer_label() will no-op until peek_peer_ghost() returns true.
bool install();
void uninstall();

// Called from hooked_OnUpdate every frame. Queues the in-playfield
// "P2" label at the peer's most recently received world position.
// Currently a text label; 6d.3 will swap in an AnmVm sprite.
void enqueue_peer_label();

// Called from hooked_GetInput every input poll. Queues only the
// top-strip "NET OK rtt=X P2 L# B# Pw# S#" status line. Split out
// from enqueue_peer_label() so it renders at title screen too —
// GetInput ticks on the title menu while OnUpdate (the GameManager
// trampoline) does not.
void enqueue_status_strip();

}  // namespace th08_platform::state::peer_ghost
