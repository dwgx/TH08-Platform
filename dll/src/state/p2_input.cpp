#include "p2_input.h"

#include <windows.h>
#include <MinHook.h>

#include <atomic>
#include <cstdint>
#include <cstring>

#include "../logging.h"
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

int __fastcall hooked_OnUpdate(void* this_, void* edx)
{
    // P1 (or any non-g_Player2 invocation): pass through.
    if (this_ != th08_platform::player2::g_Player2) {
        return g_original(this_, edx);
    }

    const unsigned long long ncalls =
        g_p2_calls.fetch_add(1, std::memory_order_relaxed) + 1;

    // Snapshot P1's input globals.
    auto* const cur  = reinterpret_cast<std::uint16_t*>(globals::kAddr_g_CurFrameInput);
    auto* const last = reinterpret_cast<std::uint16_t*>(globals::kAddr_g_LastFrameInput);
    auto* const held = reinterpret_cast<std::uint16_t*>(globals::kAddr_g_NumOfFramesInputsWereHeld);
    auto* const eighth = reinterpret_cast<std::uint16_t*>(globals::kAddr_g_IsEighthFrameOfHeldInput);
    const std::uint16_t saved_cur = *cur;
    const std::uint16_t saved_last = *last;
    const std::uint16_t saved_held = *held;
    const std::uint16_t saved_eighth = *eighth;

    // Compute P2 input.
    std::uint16_t p2_cur = 0;
    switch (g_mode) {
        case Mode::Mirror:     p2_cur = saved_cur; break;
        case Mode::Demo:       p2_cur = demo_input_for_frame(ncalls); break;
        case Mode::Stationary: p2_cur = 0; break;
    }

    // Apply P2 input. We zero held/eighth so P2 starts each frame as
    // a fresh-press case (Player movement code uses these for repeat
    // detection; for our static modes the wrong values would hurt P2
    // movement physics). Mirror mode keeps everything verbatim.
    if (g_mode == Mode::Mirror) {
        // Already saved_*; nothing to swap.
    } else {
        *cur = p2_cur;
        *last = 0;
        *held = (p2_cur == 0) ? 0 : 1;
        *eighth = 0;
    }

    const int result = g_original(this_, edx);

    // Restore P1 input globals.
    if (g_mode != Mode::Mirror) {
        *cur = saved_cur;
        *last = saved_last;
        *held = saved_held;
        *eighth = saved_eighth;
    }

    return result;
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
