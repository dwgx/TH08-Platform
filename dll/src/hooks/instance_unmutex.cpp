#include "instance_unmutex.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace th08_platform::hooks {

namespace {

// th08.exe v1.00d IAT entry for KERNEL32!CreateMutexA, located via IDA
// (sub_443420 @ 0x443420 calls [0x4b4098] = imp_CreateMutexA).
constexpr std::uintptr_t kCreateMutexAIatAddr = 0x4b4098;

// String constant ZUN passes as lpName. Hardcoded in th08.exe; matched
// exactly so we don't disturb any other (future) named-mutex callers.
constexpr const char* kZunMutexName = "Touhou 08 App";

using CreateMutexA_t = HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES, BOOL, LPCSTR);

CreateMutexA_t g_real_CreateMutexA = nullptr;
bool g_patch_applied = false;

HANDLE WINAPI hook_CreateMutexA(LPSECURITY_ATTRIBUTES attrs, BOOL initial,
                                LPCSTR name)
{
    if (name && std::strcmp(name, kZunMutexName) == 0) {
        char unique[64] = {};
        std::snprintf(unique, sizeof(unique),
                      "th08_platform_inst_%lu",
                      static_cast<unsigned long>(::GetCurrentProcessId()));
        return g_real_CreateMutexA(attrs, initial, unique);
    }
    return g_real_CreateMutexA(attrs, initial, name);
}

}  // namespace

bool patch_create_mutex_iat()
{
    auto* iat_slot = reinterpret_cast<CreateMutexA_t*>(kCreateMutexAIatAddr);

    DWORD old_protect = 0;
    if (!::VirtualProtect(iat_slot, sizeof(*iat_slot),
                          PAGE_READWRITE, &old_protect)) {
        return false;
    }

    g_real_CreateMutexA = *iat_slot;
    *iat_slot = &hook_CreateMutexA;

    DWORD restore = 0;
    ::VirtualProtect(iat_slot, sizeof(*iat_slot), old_protect, &restore);

    g_patch_applied = (g_real_CreateMutexA != nullptr);
    return g_patch_applied;
}

bool was_create_mutex_iat_patched()
{
    return g_patch_applied;
}

}  // namespace th08_platform::hooks
