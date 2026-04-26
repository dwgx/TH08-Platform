#include "p2_input.h"

#include <windows.h>
#include <MinHook.h>

#include <atomic>
#include <cstdint>
#include <cstring>

#include "../logging.h"
#include "../net/lockstep.h"
#include "globals.h"
#include "player2.h"

namespace th08_platform::state::p2_input {

namespace {

// Player::OnUpdate is `unknown` in mapping.csv but the chain
// dispatcher invokes it via `mov ecx, [elem+0x1C]; call [elem+...]`,
// passing this in ECX. So __thiscall(Player*) -> int. We hook it as
// __fastcall(Player*, void* edx_unused) which is ABI-compatible.
constexpr DWORD kPlayerOnUpdateRVA = 0x44c390 - 0x400000;

using PlayerOnUpdate_t = int(__fastcall*)(void* this_, void* edx_unused);
PlayerOnUpdate_t g_original = nullptr;

enum class Mode {
    Stationary,
    Mirror,
    Demo,
    Network,  // Phase 7: pull P2 input bits from net::peer_latest_input()
};
Mode g_mode = Mode::Stationary;
std::atomic<unsigned long long> g_p2_calls{0};

void parse_mode_env()
{
    char buf[16] = {};
    if (GetEnvironmentVariableA("TH08_PLATFORM_P2_INPUT_MODE", buf,
                                static_cast<DWORD>(sizeof(buf))) == 0) {
        g_mode = Mode::Stationary;
        return;
    }
    if (std::strcmp(buf, "mirror") == 0) {
        g_mode = Mode::Mirror;
    } else if (std::strcmp(buf, "demo") == 0) {
        g_mode = Mode::Demo;
    } else if (std::strcmp(buf, "network") == 0 || std::strcmp(buf, "net") == 0) {
        g_mode = Mode::Network;
    } else {
        g_mode = Mode::Stationary;
    }
}

// Demo mode: cycle through right/down/left/up every 60 frames.
// TH08 input bits (from input.cpp + Controller::GetInput observations):
//   0x0001 shoot, 0x0002 bomb, 0x0004 focus, 0x0008 menu/skip,
//   0x0010 up, 0x0020 down, 0x0040 left, 0x0080 right, 0x0100..
std::uint16_t demo_input_for_frame(unsigned long long n_calls)
{
    const unsigned phase = (n_calls / 60) & 3;
    switch (phase) {
        case 0: return 0x0080;  // right
        case 1: return 0x0020;  // down
        case 2: return 0x0040;  // left
        case 3: return 0x0010;  // up
    }
    return 0;
}

// Player::sub_451640 @ 0x451640 — arms the bullet timer at this+0xE2AC4
// for shooting. Verified this-aware by codex 5.5 breakthrough RE.
// Without this, sub_451500's gate at 0x451527 (`if (*(this+0xE2AC4) < 0)
// return 0;`) blocks the call to sub_450F60 → sub_44FB70 (spawn).
constexpr std::uintptr_t kAddr_ArmShotTimer = 0x451640;
using PlayerThunk_t = void(__fastcall*)(void* this_, void* edx_unused);

// Bomb-press flag offset inside Player struct. sub_44C650 reads
// *(BYTE*)(this+6); if non-zero AND bomb count == 1, runs the bomb path.
constexpr std::ptrdiff_t kOff_BombPressFlag = 6;

// RAII guard: snapshots BOTH g_CurFrameInput (0x164D528) AND
// g_GameInputBits (0x164D52C) on construction, restores on destruction.
// Exception-safe (review HIGH p2_input.cpp:103/109): if the original
// OnUpdate SEHs through us, the dtor still runs and P1 input is
// restored.
//
// Two different input slots exist:
//   0x164D528 (g_CurFrameInput): the raw poll from Controller::GetInput
//   0x164D52C (g_GameInputBits): the processed/edge-detected variant
//                                read by sub_44AEC0 for movement,
//                                shoot detection, etc. (codex 5.5 RE)
struct InputSwapGuard {
    bool active;
    std::uint16_t* cur;
    std::uint16_t* gamebits;
    std::uint16_t* last;
    std::uint16_t* held;
    std::uint16_t* eighth;
    std::uint16_t saved_cur, saved_gamebits, saved_last, saved_held, saved_eighth;

    explicit InputSwapGuard(bool swap_now)
        : active(swap_now)
        , cur(reinterpret_cast<std::uint16_t*>(globals::kAddr_g_CurFrameInput))
        , gamebits(reinterpret_cast<std::uint16_t*>(globals::kAddr_g_GameInputBits))
        , last(reinterpret_cast<std::uint16_t*>(globals::kAddr_g_LastFrameInput))
        , held(reinterpret_cast<std::uint16_t*>(globals::kAddr_g_NumOfFramesInputsWereHeld))
        , eighth(reinterpret_cast<std::uint16_t*>(globals::kAddr_g_IsEighthFrameOfHeldInput))
        , saved_cur(*cur), saved_gamebits(*gamebits)
        , saved_last(*last), saved_held(*held), saved_eighth(*eighth)
    {}

    void apply_p2(std::uint16_t p2_cur)
    {
        *cur = p2_cur;
        *gamebits = p2_cur;          // movement / shoot / bomb consumer
        *last = 0;
        *held = (p2_cur == 0) ? 0 : 1;
        *eighth = 0;
    }

    ~InputSwapGuard()
    {
        if (active) {
            *cur = saved_cur;
            *gamebits = saved_gamebits;
            *last = saved_last;
            *held = saved_held;
            *eighth = saved_eighth;
        }
    }
};

// Bypass-the-pipeline writes for shoot + bomb. Even with input swap,
// sub_44AEC0 only writes the bomb-press flag for P1 (codex 5.5 RE
// confirmed: no `*(this+6)` write in sub_44AEC0). And sub_451500's
// shoot gate needs the timer pre-armed by sub_451640. So we directly
// poke g_Player2's slots before OnUpdate runs.
void apply_p2_action_pokes(void* p2, std::uint16_t p2_cur)
{
    if (!p2) return;
    auto* const arm = reinterpret_cast<PlayerThunk_t>(kAddr_ArmShotTimer);
    if (p2_cur & 0x0001) {  // Z / shoot — arm the bullet timer this frame
        __try {
            arm(p2, nullptr);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // Arming on a fresh-ctor'd Player buffer might trip if
            // chain isn't fully wired; swallow + skip rather than die.
        }
    }
    // X / bomb — set the press flag this frame; cleared by sub_44C650
    // when consumed (or stays 1 next frame if still held; that's fine
    // because sub_44C650 also requires bomb-count > 0 to act).
    auto* const bomb_flag = reinterpret_cast<std::uint8_t*>(
        reinterpret_cast<std::uint8_t*>(p2) + kOff_BombPressFlag);
    *bomb_flag = (p2_cur & 0x0002) ? 1 : 0;
}

int __fastcall hooked_OnUpdate(void* this_, void* edx)
{
    // P1 (or any non-g_Player2 invocation): pass through.
    if (this_ != th08_platform::player2::g_Player2) {
        return g_original(this_, edx);
    }

    const unsigned long long ncalls =
        g_p2_calls.fetch_add(1, std::memory_order_relaxed) + 1;

    // Compute P2 input.
    std::uint16_t p2_cur = 0;
    switch (g_mode) {
        case Mode::Mirror:     /* keep P1 input verbatim - no swap */ break;
        case Mode::Demo:       p2_cur = demo_input_for_frame(ncalls); break;
        case Mode::Network:    p2_cur = th08_platform::net::peer_latest_input(); break;
        case Mode::Stationary: p2_cur = 0; break;
    }

    // Mirror mode = use P1 input verbatim for swap targets, but still
    // poke the action slots so P2 fires/bombs when P1 does.
    if (g_mode == Mode::Mirror) {
        // P1's already-current input drives both players' moveDirection;
        // sub_44AEC0 will run for both with the same input. We still
        // bypass the input pipeline for shoot/bomb on P2 because the
        // shoot timer + bomb flag aren't written by sub_44AEC0 even
        // when input is "correct".
        const std::uint16_t p1_cur = *reinterpret_cast<std::uint16_t*>(globals::kAddr_g_CurFrameInput);
        apply_p2_action_pokes(this_, p1_cur);
        return g_original(this_, edx);
    }

    InputSwapGuard guard(/*swap_now=*/true);
    guard.apply_p2(p2_cur);
    apply_p2_action_pokes(this_, p2_cur);
    return g_original(this_, edx);
    // ~InputSwapGuard restores even on SEH unwind.
}

void* resolve_target()
{
    HMODULE base = GetModuleHandleA(nullptr);
    if (!base) return nullptr;
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uint8_t*>(base) + kPlayerOnUpdateRVA);
}

}  // namespace

bool install_hook()
{
    parse_mode_env();
    const char* mode_str = (g_mode == Mode::Mirror)     ? "mirror"
                         : (g_mode == Mode::Demo)       ? "demo"
                         : (g_mode == Mode::Network)    ? "network"
                                                        : "stationary";
    log_line("p2_input: mode=%s (env TH08_PLATFORM_P2_INPUT_MODE)", mode_str);

    // MH_Initialize is owned by install_game_loop_hook (called first).
    void* target = resolve_target();
    if (!target) {
        log_line("p2_input: resolve_target returned null");
        return false;
    }
    log_line("p2_input: hooking Player::OnUpdate @ %p", target);

    if (MH_CreateHook(target, reinterpret_cast<void*>(&hooked_OnUpdate),
                      reinterpret_cast<LPVOID*>(&g_original)) != MH_OK) {
        log_line("p2_input: MH_CreateHook failed");
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        log_line("p2_input: MH_EnableHook failed");
        return false;
    }
    log_line("p2_input: enabled");
    return true;
}

void uninstall_hook()
{
    if (g_original == nullptr) return;
    void* target = resolve_target();
    if (target != nullptr) {
        MH_DisableHook(target);
        MH_RemoveHook(target);
    }
    g_original = nullptr;
}

}  // namespace th08_platform::state::p2_input
