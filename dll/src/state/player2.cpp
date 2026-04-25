#include "player2.h"

#include <windows.h>

#include "../logging.h"

#include <cstring>

namespace th08_platform::player2 {
namespace {

// Player::Player ctor address in th08.exe v1.00d.
// Verified: mapping.csv row 746 -> th08::Player::Player @ 0x449ca0,
// __thiscall, size 0x1A6. The function takes `this` in ECX and returns
// `this` in EAX (standard MSVC __thiscall constructor convention).
constexpr std::uintptr_t kAddr_Player_ctor = 0x449ca0;

using PlayerCtor_t = void* (__thiscall*)(void* this_);

bool g_constructed = false;

}  // namespace

alignas(16) std::uint8_t g_Player2[kSize] = {};

bool Construct()
{
    if (g_constructed) {
        // Re-ctor is allowed: zero the buffer and run ctor again. Useful
        // for test harness state reset.
        std::memset(g_Player2, 0, kSize);
        g_constructed = false;
    } else {
        std::memset(g_Player2, 0, kSize);
    }

    // Sanity: refuse to ctor if the ctor address is not in th08.exe's
    // .text region. Cheap protection against being injected into a
    // process that isn't th08.
    MEMORY_BASIC_INFORMATION mbi{};
    const SIZE_T queried = VirtualQuery(reinterpret_cast<LPCVOID>(kAddr_Player_ctor),
                                        &mbi, sizeof(mbi));
    if (queried == 0 || mbi.State != MEM_COMMIT ||
        (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                        PAGE_EXECUTE_WRITECOPY)) == 0) {
        log_line("player2: ctor address 0x%08X is not in an executable page; abort",
                 static_cast<unsigned>(kAddr_Player_ctor));
        return false;
    }

    auto* const ctor = reinterpret_cast<PlayerCtor_t>(kAddr_Player_ctor);

    // Wrap in SEH so a ctor crash (e.g. AnmManager not yet initialized,
    // or sub_406720 dereferencing uninit g_BulletManager) becomes a
    // clean false return rather than killing the game process.
    bool ok = false;
    __try {
        ctor(g_Player2);
        ok = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_line("player2: Player::Player ctor raised SEH; g_Player2 left zeroed");
        std::memset(g_Player2, 0, kSize);
        ok = false;
    }

    if (ok) {
        g_constructed = true;
        log_line("player2: g_Player2 constructed at %p (size 0x%X)",
                 static_cast<void*>(g_Player2), static_cast<unsigned>(kSize));
        // Sanity: playerState should be 0 (PLAYER_STATE_ALIVE) since
        // ZUN's ctor doesn't write it - it stays at the memset(0) value.
        log_line("player2: post-ctor playerState=%d (expected 0)",
                 static_cast<int>(g_Player2[0]));
    }

    return ok;
}

void Destruct()
{
    if (!g_constructed) {
        return;
    }
    // ZUN's Player has no explicit dtor; the two heap slots at
    // +0xE2A74/+0xE2A78 are populated by AddedCallback (NOT the ctor)
    // so 5a never allocates them. When 5b calls AddedCallback for
    // g_Player2, it will need a matching DeletedCallback path on
    // shutdown. Until then this is intentionally a no-op.
    std::memset(g_Player2, 0, kSize);
    g_constructed = false;
    log_line("player2: g_Player2 destructed (zeroed)");
}

bool IsConstructed() noexcept
{
    return g_constructed;
}

}  // namespace th08_platform::player2
