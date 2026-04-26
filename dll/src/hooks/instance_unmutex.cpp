#include "instance_unmutex.h"

#include <windows.h>

#include <cstdint>

namespace th08_platform::hooks {

namespace {

// th08.exe v1.00d, in sub_443420 @ 0x443420:
//   0x44344A  83 F8 B7        cmp eax, 0B7h
//   0x44344F  75 1A           jnz short loc_44346B
//   0x443451  68 C8 64 4B 00  push offset "二つは起動できません..."
//
// We change the byte at 0x44344F from 0x75 (JNZ short) to 0xEB (JMP short).
// Both opcodes are 2-byte short branches with the same encoding for the
// 8-bit displacement, so the operand 0x1A continues to mean "skip 0x1A
// bytes forward to loc_44346B" — the success path.
constexpr std::uintptr_t kJnzAddr = 0x44344F;
constexpr std::uint8_t kJnzOpcode = 0x75;
constexpr std::uint8_t kJmpOpcode = 0xEB;

bool g_patch_applied = false;

}  // namespace

bool patch_zun_single_instance_check()
{
    auto* target = reinterpret_cast<std::uint8_t*>(kJnzAddr);

    DWORD old_protect = 0;
    if (!::VirtualProtect(target, 1, PAGE_EXECUTE_READWRITE, &old_protect)) {
        return false;
    }

    const std::uint8_t found = *target;
    if (found == kJmpOpcode) {
        // Already patched (idempotent — should never happen since DLL
        // is loaded once per process, but guard anyway).
        DWORD restore = 0;
        ::VirtualProtect(target, 1, old_protect, &restore);
        g_patch_applied = true;
        return true;
    }
    if (found != kJnzOpcode) {
        // Unexpected byte — bail without writing to avoid corrupting
        // an unrelated th08 build / version.
        DWORD restore = 0;
        ::VirtualProtect(target, 1, old_protect, &restore);
        return false;
    }

    *target = kJmpOpcode;

    // Flush instruction cache so the CPU sees the patched byte. Required
    // on x86 for code self-modification correctness.
    ::FlushInstructionCache(::GetCurrentProcess(), target, 1);

    DWORD restore = 0;
    ::VirtualProtect(target, 1, old_protect, &restore);

    g_patch_applied = true;
    return true;
}

bool was_zun_single_instance_check_patched()
{
    return g_patch_applied;
}

}  // namespace th08_platform::hooks
