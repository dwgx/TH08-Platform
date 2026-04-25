#pragma once

// Sub-phase 5d: per-player input routing.
//
// Hooks `Player::OnUpdate @ 0x44C390`. When the chain dispatcher calls
// it with this=&g_Player2, our detour SAVES the current input globals
// (g_CurFrameInput / g_LastFrameInput / g_NumOfFramesInputsWereHeld /
// g_IsEighthFrameOfHeldInput), OVERWRITES them with P2's input source,
// runs the original OnUpdate, then RESTORES P1's input globals.
//
// The save/restore window is just one OnUpdate call, so any code
// outside Player::OnUpdate that also reads input globals (e.g. menu
// code, pause check) sees only P1's input - which is correct.
//
// P2 input source modes (env TH08_PLATFORM_P2_INPUT_MODE):
//   "stationary" (default) - P2 input is always 0; P2 stays put
//                            wherever AddedCallback / spawn-shift left
//                            it. Useful for visual confirmation that
//                            the routing works.
//   "mirror"               - P2 input == P1 input verbatim. Both
//                            players move together. Confirms P2's
//                            OnUpdate path sees and acts on input.
//   "demo"                 - P2 cycles through a fixed direction
//                            pattern (right 60f, down 60f, left 60f,
//                            up 60f). Visually obvious the routing
//                            works without needing P2 keyboard.
//
// 5d does NOT yet read a real second input device. That comes later
// (probably by hooking IDirectInputDevice8::GetDeviceState for a
// joystick or by reserving a key range like IJKL on the keyboard).

namespace th08_platform::state::p2_input {

bool install_hook();
void uninstall_hook();

}  // namespace th08_platform::state::p2_input
