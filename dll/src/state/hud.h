#pragma once

// Sub-phase 5g: visual HUD for P2.
//
// Each frame, before ZUN's draw chain runs, queue text strings into
// AsciiManager (g_AsciiManager @ 0x4CCE20) via its AddString primitive
// (mapping.csv row 15: __thiscall(AsciiManager*, D3DXVECTOR3* pos,
// char* text) @ 0x402920). AsciiManager's OnDrawHighPrio (registered
// in chain) flushes the queue via DrawStrings @ 0x402B20 and clears
// it. So adding strings each frame means they render each frame.
//
// We render to the right side of the play-area boundary (TH08 plays
// in 384x448 px on the left of a 640-wide window; the right side is
// score / lives / etc). P2 stats render below P1's stats at a
// configurable y-offset.
//
// Default off; opt-in via env TH08_PLATFORM_HUD=1.

namespace th08_platform::state::hud {

bool install();
void uninstall();

// Called from GameManager::OnUpdate hook each frame to enqueue P2
// strings into AsciiManager. Strings persist for one frame and are
// flushed by AsciiManager::OnDrawHighPrio during the draw chain.
void enqueue_p2_strings();

}  // namespace th08_platform::state::hud
