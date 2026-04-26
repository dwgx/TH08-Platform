#pragma once

namespace th08_platform::hooks {

// Defeat th08.exe v1.00d's hardcoded single-instance check at sub_443420.
// ZUN calls CreateMutexA(NULL, TRUE, "Touhou 08 App") then exits if
// GetLastError() == ERROR_ALREADY_EXISTS. We can't easily IAT-hook the
// CreateMutexA call (the IAT entry at 0x4b4098 looked correct but the
// real runtime resolution path bypassed our hook on the second instance —
// likely a TLS callback or OS re-resolution edge case). Instead we patch
// the conditional branch right after the GetLastError check:
//
//   0x44344A  cmp eax, 0B7h         ; ERROR_ALREADY_EXISTS = 183
//   0x44344F  jnz short loc_44346B  ; bytes: 75 1A
//   0x443451  push offset "..."     ; collision dialog path
//
// Flip 0x75 (JNZ) -> 0xEB (JMP short, same operand) at 0x44344F so the
// branch is always taken to the success path. CreateMutexA still runs
// (returns a valid handle even on collision; ZUN's null-check at 0x44367C
// passes), the dialog path is skipped, both instances proceed.
//
// Must run BEFORE main thread resumes (called from DllMain
// DLL_PROCESS_ATTACH). The patch is one byte + VirtualProtect — safe
// for DllMain context, no loader-lock concerns.
bool patch_zun_single_instance_check();

// True after a successful patch. Read from dll_init_thread for logging.
bool was_zun_single_instance_check_patched();

}  // namespace th08_platform::hooks
