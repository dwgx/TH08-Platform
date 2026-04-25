#include "player2.h"

#include <windows.h>

#include "../logging.h"
#include "globals.h"

#include <cstring>

namespace th08_platform::player2 {
namespace {

// Player::Player ctor address in th08.exe v1.00d.
// Verified: mapping.csv row 746 -> th08::Player::Player @ 0x449ca0,
// __thiscall, size 0x1A6. The function takes `this` in ECX and returns
// `this` in EAX (standard MSVC __thiscall constructor convention).
constexpr std::uintptr_t kAddr_Player_ctor = 0x449ca0;

// Chain primitives, all from mapping.csv:
//   row 557: Chain::AddToCalcChain @ 0x43c880, __thiscall(Chain*, ChainElem*, u32)
//   row 558: Chain::AddToDrawChain @ 0x43c960, __thiscall(Chain*, ChainElem*, i32)
//   row 564: Chain::CreateElem    @ 0x43ce60, __thiscall(Chain*, u32) -> ChainElem*
//            (the u32 is reinterpreted as the func_ptr to register)
//   row 565: Chain::Cut           @ 0x43cf10, __thiscall(Chain*, ChainElem*)
// Chain element layout (verified from RegisterChain @ 0x44C230):
//   +0x08 = AddedCallback function pointer
//   +0x0C = DeletedCallback function pointer
//   +0x1C = context pointer (target of OnUpdate's `this`)
constexpr std::uintptr_t kAddr_CreateElem      = 0x43ce60;
constexpr std::uintptr_t kAddr_AddToCalcChain  = 0x43c880;
constexpr std::uintptr_t kAddr_AddToDrawChain  = 0x43c960;
constexpr std::uintptr_t kAddr_ChainCut        = 0x43cf10;
constexpr std::uintptr_t kAddr_Player_OnUpdate         = 0x44c390;
constexpr std::uintptr_t kAddr_Player_OnDrawHighPrio   = 0x44d530;
constexpr std::uintptr_t kAddr_Player_OnDrawLowPrio    = 0x44d630;
constexpr std::uintptr_t kAddr_Player_AddedCallback    = 0x44d650;
constexpr std::uintptr_t kAddr_Player_DeletedCallback  = 0x44dc60;
constexpr std::ptrdiff_t kOff_ChainElem_AddedCb  = 0x08;
constexpr std::ptrdiff_t kOff_ChainElem_DeletedCb = 0x0c;
constexpr std::ptrdiff_t kOff_ChainElem_Context  = 0x1c;
constexpr std::int32_t kPriority_PlayerCalc      = 9;
constexpr std::int32_t kPriority_PlayerDrawHigh  = 9;
constexpr std::int32_t kPriority_PlayerDrawLow   = 10;

using PlayerCtor_t       = void*(__thiscall*)(void* this_);
using CreateElem_t       = void*(__thiscall*)(void* chain_this, std::uint32_t func_or_id);
using AddToCalcChain_t   = std::int32_t(__thiscall*)(void* chain_this, void* elem, std::uint32_t prio);
using AddToDrawChain_t   = std::int32_t(__thiscall*)(void* chain_this, void* elem, std::int32_t prio);
using ChainCut_t         = void(__thiscall*)(void* chain_this, void* elem);

bool g_constructed = false;
bool g_registered = false;

void* g_calcChain      = nullptr;
void* g_drawHighChain  = nullptr;
void* g_drawLowChain   = nullptr;

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

bool IsEligible() noexcept
{
    if (!g_constructed) {
        return false;
    }
    // Player state byte at +0x0000. 0=alive, 1=spawning, 2=dead,
    // 3=respawn-ghost, 4=respawn-related (per player_layout.h).
    const std::uint8_t state = g_Player2[0];
    return state != 2 && state != 3;
}

namespace {

// Helper: write a function-pointer field at ChainElem + offset. Wrapped
// so the writes are obvious in code review.
void set_chain_callback(void* elem, std::ptrdiff_t off, std::uintptr_t addr) noexcept
{
    *reinterpret_cast<std::uintptr_t*>(reinterpret_cast<std::uint8_t*>(elem) + off) = addr;
}

}  // namespace

bool Register()
{
    if (!g_constructed) {
        log_line("player2: Register() called before Construct(); abort");
        return false;
    }
    if (g_registered) {
        log_line("player2: Register() called twice without Unregister; treating as no-op");
        return true;
    }

    log_line("player2: WARNING - chain registration without AddedCallback "
             "will likely crash on first tick. Proceeding because env opted in.");

    auto* const create_elem = reinterpret_cast<CreateElem_t>(kAddr_CreateElem);
    auto* const add_calc    = reinterpret_cast<AddToCalcChain_t>(kAddr_AddToCalcChain);
    auto* const add_draw    = reinterpret_cast<AddToDrawChain_t>(kAddr_AddToDrawChain);
    void* const chain_this  = reinterpret_cast<void*>(globals::kAddr_g_Chain);

    bool ok = false;
    __try {
        g_calcChain     = create_elem(chain_this, static_cast<std::uint32_t>(kAddr_Player_OnUpdate));
        g_drawHighChain = create_elem(chain_this, static_cast<std::uint32_t>(kAddr_Player_OnDrawHighPrio));
        g_drawLowChain  = create_elem(chain_this, static_cast<std::uint32_t>(kAddr_Player_OnDrawLowPrio));

        if (g_calcChain == nullptr || g_drawHighChain == nullptr || g_drawLowChain == nullptr) {
            log_line("player2: chain elem alloc failed (calc=%p high=%p low=%p)",
                     g_calcChain, g_drawHighChain, g_drawLowChain);
        } else {
            // Wire context pointer + lifecycle callbacks on the calc chain
            // (matches what Player::RegisterChain does for g_Player).
            set_chain_callback(g_calcChain,     kOff_ChainElem_Context,
                               reinterpret_cast<std::uintptr_t>(g_Player2));
            set_chain_callback(g_calcChain,     kOff_ChainElem_AddedCb,
                               kAddr_Player_AddedCallback);
            set_chain_callback(g_calcChain,     kOff_ChainElem_DeletedCb,
                               kAddr_Player_DeletedCallback);
            // Draw chain elements just need the context pointer.
            set_chain_callback(g_drawHighChain, kOff_ChainElem_Context,
                               reinterpret_cast<std::uintptr_t>(g_Player2));
            set_chain_callback(g_drawLowChain,  kOff_ChainElem_Context,
                               reinterpret_cast<std::uintptr_t>(g_Player2));

            const std::int32_t r1 = add_calc(chain_this, g_calcChain, kPriority_PlayerCalc);
            const std::int32_t r2 = add_draw(chain_this, g_drawHighChain, kPriority_PlayerDrawHigh);
            const std::int32_t r3 = add_draw(chain_this, g_drawLowChain, kPriority_PlayerDrawLow);
            if (r1 == 0 && r2 == 0 && r3 == 0) {
                ok = true;
            } else {
                log_line("player2: chain Add returned nonzero (calc=%d high=%d low=%d)",
                         r1, r2, r3);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_line("player2: SEH during chain register; partial state left for Unregister to clean");
        ok = false;
    }

    if (ok) {
        g_registered = true;
        log_line("player2: registered with chain (calc=%p drawHigh=%p drawLow=%p)",
                 g_calcChain, g_drawHighChain, g_drawLowChain);
    } else {
        Unregister();
    }
    return ok;
}

void Unregister()
{
    // Chain::Cut (mapping.csv row 565 @ 0x43cf10) removes a ChainElem
    // from the chain's linked list. Safe to call even if the elem isn't
    // currently in the chain (Cut is idempotent per RegisterChain
    // semantics; CutImpl at 0x43cf50 is the worker).
    auto* const chain_cut = reinterpret_cast<ChainCut_t>(kAddr_ChainCut);
    void* const chain_this = reinterpret_cast<void*>(globals::kAddr_g_Chain);

    __try {
        if (g_calcChain != nullptr)     chain_cut(chain_this, g_calcChain);
        if (g_drawHighChain != nullptr) chain_cut(chain_this, g_drawHighChain);
        if (g_drawLowChain != nullptr)  chain_cut(chain_this, g_drawLowChain);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_line("player2: SEH during Chain::Cut; chain may be in inconsistent state");
    }

    if (g_registered) {
        log_line("player2: unregistered from chain");
    }
    g_registered = false;
    g_calcChain = nullptr;
    g_drawHighChain = nullptr;
    g_drawLowChain = nullptr;
}

bool IsRegistered() noexcept
{
    return g_registered;
}

}  // namespace th08_platform::player2
