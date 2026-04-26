#include "peer_ghost.h"

#include "../logging.h"
#include "../net/lockstep.h"
#include "globals.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>

namespace th08_platform::state::peer_ghost {

namespace {

// AsciiManager::AddString @ 0x402920. Same address Phase 5g HUD uses;
// thiscall ABI with EDX unused, so MSVC __fastcall on (this, edx_unused,
// &pos, text) matches.
constexpr DWORD kAddString_RVA = 0x402920 - 0x400000;

using AddString_t = void(__fastcall*)(void* this_, void* edx_unused,
                                      const float* pos, const char* text);

AddString_t g_AddString = nullptr;
bool g_enabled = false;

// TH08 play area is at screen-coords (32, 16) origin, 384×448. World
// coords stored in g_Player.pos are pixel-like in that local frame.
// Add the origin offset to land the label inside the playfield.
constexpr float kPlayfieldOriginX = 32.0f;
constexpr float kPlayfieldOriginY = 16.0f;

void* resolve_addstring()
{
    HMODULE base = GetModuleHandleA(nullptr);
    if (!base) return nullptr;
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uint8_t*>(base) + kAddString_RVA);
}

}  // namespace

bool install()
{
    g_AddString = reinterpret_cast<AddString_t>(resolve_addstring());
    if (!g_AddString) {
        log_line("peer_ghost: failed to resolve AddString");
        return false;
    }
    g_enabled = true;
    log_line("peer_ghost: enabled (AddString @ %p)", g_AddString);
    return true;
}

void uninstall()
{
    g_enabled = false;
    g_AddString = nullptr;
}

void enqueue_peer_label()
{
    if (!g_enabled || !g_AddString) return;

    float wx = 0.0f, wy = 0.0f;
    std::uint64_t frame = 0;
    if (!th08_platform::net::peek_peer_ghost(wx, wy, frame)) return;

    // Skip pre-stage zero-pos so we don't draw "P2" pinned at (32,16)
    // before the peer enters a stage.
    if (wx == 0.0f && wy == 0.0f) return;

    void* const ascii = reinterpret_cast<void*>(globals::kAddr_g_AsciiManager);

    // Calls into ZUN code with our crafted args. SEH so a bad queue
    // state doesn't crash the game on every frame.
    __try {
        struct Vec3 { float x, y, z; };
        Vec3 pos = { kPlayfieldOriginX + wx, kPlayfieldOriginY + wy, 0.0f };
        g_AddString(ascii, nullptr, &pos.x, "P2");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_line("peer_ghost: SEH during AddString; disabling for safety");
        g_enabled = false;
    }
}

}  // namespace th08_platform::state::peer_ghost
