#pragma once

namespace th08_platform::state::peer_ghost {

// Resolve AsciiManager::AddString and arm the ghost-render path. Called
// from dll_init_thread once. Safe to call without a connected peer —
// enqueue_peer_label() will no-op until peek_peer_ghost() returns true.
bool install();
void uninstall();

// Called from hooked_OnUpdate every frame. Queues an AsciiManager
// label at the peer's most recently received world position so it
// renders this frame. Currently a text label "P2"; 6d.3 will swap
// in an AnmVm sprite if the text-only render proves too crude.
void enqueue_peer_label();

}  // namespace th08_platform::state::peer_ghost
