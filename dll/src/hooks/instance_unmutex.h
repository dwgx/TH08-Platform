#pragma once

namespace th08_platform::hooks {

// Patch th08.exe's IAT entry for KERNEL32!CreateMutexA so that the named
// "Touhou 08 App" mutex (used by ZUN as a single-instance guard) becomes
// per-process unique. Without this, the second loader-launched th08.exe
// hits ERROR_ALREADY_EXISTS in sub_443420 @ 0x443420 and silently exits.
//
// Must be called from DllMain DLL_PROCESS_ATTACH (before the main thread
// is resumed by the loader) so the patch is in place by the time th08's
// startup path calls CreateMutexA.
bool patch_create_mutex_iat();

// True after a successful patch_create_mutex_iat() call. Safe to read
// from dll_init_thread to drive a startup log line.
bool was_create_mutex_iat_patched();

}  // namespace th08_platform::hooks
