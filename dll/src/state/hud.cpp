#include "hud.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

#include "../logging.h"
#include "globals.h"
#include "player2.h"
#include "p2_lives.h"
#include "dual_collision.h"

namespace th08_platform::state::hud {

namespace {

// AsciiManager API (mapping.csv):
//   row  15: AddString @ 0x402920  __thiscall(this, D3DXVECTOR3* pos, char* text)
constexpr DWORD kAddString_RVA = 0x402920 - 0x400000;

// MSVC __fastcall mimics __thiscall ABI for the first arg in ECX.
using AddString_t = void(__fastcall*)(void* this_, void* edx_unused,
                                      const float* pos, const char* text);

AddString_t g_AddString = nullptr;
bool g_enabled = false;

// HUD position. TH08 window is 640x480, play area is 32..32+384 = 32..416
// horizontally, 16..16+448 = 16..464 vertically. Right HUD column starts
// around x=420, P1 stats span y=80-340 roughly. Put P2 stats further down
// at y=360+ (below P1's gauge).
struct Vec3 { float x, y, z; };

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
        log_line("hud: failed to resolve AddString");
        return false;
    }
    g_enabled = true;
    log_line("hud: enabled (AddString @ %p, AsciiManager @ %p)",
             g_AddString, reinterpret_cast<void*>(globals::kAddr_g_AsciiManager));
    return true;
}

void uninstall()
{
    g_enabled = false;
    g_AddString = nullptr;
}

void enqueue_p2_strings()
{
    if (!g_enabled || !g_AddString) return;

    void* const ascii = reinterpret_cast<void*>(globals::kAddr_g_AsciiManager);
    auto* const p2 = th08_platform::player2::g_Player2;
    if (!p2 || !th08_platform::player2::IsConstructed()) return;

    const int p2_lives = th08_platform::state::p2_lives::snapshot_p2_lives();
    const int p2_state = static_cast<int>(p2[0]);
    const auto hits = th08_platform::state::dual_collision::snapshot_hit_counters();

    // Read P2 pos (verified +0x2B4) for an "alive at (x,y)" indicator.
    const float pos_x = *reinterpret_cast<float*>(p2 + 0x2B4);
    const float pos_y = *reinterpret_cast<float*>(p2 + 0x2B8);

    // SEH-wrap because we're calling ZUN code with our own args; if the
    // AddString queue is full or anything goes weird, don't take down
    // the game.
    __try {
        char buf[64];

        // Line 1: "P2 LIVES: N"
        Vec3 pos1 = {424.0f, 360.0f, 0.0f};
        std::snprintf(buf, sizeof(buf), "P2 LIVES %d", p2_lives);
        g_AddString(ascii, nullptr, &pos1.x, buf);

        // Line 2: "P2 STATE: S" (0=alive, 1=spawning, 2=dead, 3=ghost)
        Vec3 pos2 = {424.0f, 380.0f, 0.0f};
        const char* state_name =
            p2_state == 0 ? "alive" :
            p2_state == 1 ? "spawn" :
            p2_state == 2 ? "DEAD"  :
            p2_state == 3 ? "ghost" : "?";
        std::snprintf(buf, sizeof(buf), "P2 ST %s", state_name);
        g_AddString(ascii, nullptr, &pos2.x, buf);

        // Line 3: "P2 HITS: N" (since session start)
        Vec3 pos3 = {424.0f, 400.0f, 0.0f};
        std::snprintf(buf, sizeof(buf), "P2 HITS %llu", hits.p2_hits);
        g_AddString(ascii, nullptr, &pos3.x, buf);

        // Line 4: P2 position
        Vec3 pos4 = {424.0f, 420.0f, 0.0f};
        std::snprintf(buf, sizeof(buf), "P2 @ %.0f,%.0f", pos_x, pos_y);
        g_AddString(ascii, nullptr, &pos4.x, buf);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_line("hud: SEH during AddString; disabling HUD for safety");
        g_enabled = false;
    }
}

}  // namespace th08_platform::state::hud
