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

void enqueue_status_strip()
{
    if (!g_enabled || !g_AddString) return;

    void* const ascii = reinterpret_cast<void*>(globals::kAddr_g_AsciiManager);
    struct Vec3 { float x, y, z; };

    __try {
        char status[128];
        const bool connected = th08_platform::net::is_connected();
        const auto rtt = th08_platform::net::last_rtt_ms();
        const auto pl = th08_platform::net::peer_ghost_lives();
        const auto pb = th08_platform::net::peer_ghost_bombs();
        const auto pp = th08_platform::net::peer_ghost_power();
        const auto ps = th08_platform::net::peer_ghost_score();
        // Phase 6f: peer's game phase. ghost packet's frame == 0 means
        // peer is still at title screen; > 0 means in stage at that frame.
        float gx = 0.0f, gy = 0.0f;
        std::uint64_t pf = 0;
        const bool got_ghost = th08_platform::net::peek_peer_ghost(gx, gy, pf);
        const char* phase = !got_ghost ? "WAIT"
                          : (pf == 0)  ? "TITLE"
                          :              "STAGE";
        // Phase 6g.1: show local + peer RNG state side by side. If they
        // diverge once both peers are in stage, that's a desync.
        const auto local_rng =
            *reinterpret_cast<const std::uint16_t*>(0x0164D520);
        const auto peer_rng = th08_platform::net::peer_ghost_rng();
        std::snprintf(status, sizeof(status),
                      "NET %s %llums  P2 %s f=%llu L%u B%u Pw%u S%u  RNG %04x/%04x",
                      connected ? "OK" : "..",
                      static_cast<unsigned long long>(rtt),
                      phase,
                      static_cast<unsigned long long>(pf),
                      static_cast<unsigned>(pl), static_cast<unsigned>(pb),
                      static_cast<unsigned>(pp), static_cast<unsigned>(ps),
                      static_cast<unsigned>(local_rng),
                      static_cast<unsigned>(peer_rng));
        Vec3 status_pos = { 424.0f, 10.0f, 0.0f };
        g_AddString(ascii, nullptr, &status_pos.x, status);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_line("peer_ghost: SEH during status AddString; disabling for safety");
        g_enabled = false;
    }
}

void enqueue_peer_label()
{
    if (!g_enabled || !g_AddString) return;

    float wx = 0.0f, wy = 0.0f;
    std::uint64_t frame = 0;
    if (!th08_platform::net::peek_peer_ghost(wx, wy, frame)) return;
    if (wx == 0.0f && wy == 0.0f) return;  // pre-stage, skip

    void* const ascii = reinterpret_cast<void*>(globals::kAddr_g_AsciiManager);
    struct Vec3 { float x, y, z; };

    __try {
        Vec3 ghost_pos = { kPlayfieldOriginX + wx, kPlayfieldOriginY + wy, 0.0f };
        g_AddString(ascii, nullptr, &ghost_pos.x, "P2");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log_line("peer_ghost: SEH during ghost AddString; disabling for safety");
        g_enabled = false;
    }
}

}  // namespace th08_platform::state::peer_ghost
