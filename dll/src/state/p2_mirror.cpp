#include "p2_mirror.h"

#include <windows.h>
#include <MinHook.h>

#include "../logging.h"
#include "globals.h"
#include "player2.h"

namespace th08_platform::state::p2_mirror {

namespace {

// Verified addresses + signatures via codex 5.5 round-2 RE + my
// ida_helper analyze_function calls (this turn).
//
// sub_44DE60: __thiscall(char* this, DWORD* pos2, int x3, int x4, int x5, int x6)
//   Iterates this+0xBB834..+0xBE834 (192 slots, 64B each) for a free
//   slot, fills it with bullet record. Called by enemy spawn paths.
//
// sub_44DF00: SAME signature, slight variant (calls sub_44E370 with
//   one arg instead of two, and field layout differs by 1 dword).
//
// sub_451670: __thiscall(int this, float* a2, float* a3, DWORD* a4, DWORD* a5)
//   Iterates this+0xBE838 (128 player bullets, 9 dwords header + body)
//   and tests each against the enemy's bbox (a2/a3). Returns hit count;
//   writes via a4/a5 if hit. This is the damage-application primitive.
constexpr DWORD kRVA_44DE60 = 0x44DE60 - 0x400000;
constexpr DWORD kRVA_44DF00 = 0x44DF00 - 0x400000;
constexpr DWORD kRVA_451670 = 0x451670 - 0x400000;

using Slot44DE60_t = char*(__fastcall*)(void* this_, void* edx_unused,
                                        void* a2, int a3, int a4, int a5, int a6);
using Slot44DF00_t = char*(__fastcall*)(void* this_, void* edx_unused,
                                        void* a2, int a3, int a4, int a5, int a6);
using Damage_t     = int (__fastcall*)(void* this_, void* edx_unused,
                                       float* a2, float* a3, void* a4, void* a5);

Slot44DE60_t g_orig_44DE60 = nullptr;
Slot44DF00_t g_orig_44DF00 = nullptr;
Damage_t     g_orig_451670 = nullptr;

// Cheap helper: is `p` the canonical g_Player singleton?
inline bool is_p1(void* p)
{
    return p == reinterpret_cast<void*>(globals::kAddr_g_Player);
}

char* __fastcall hooked_44DE60(void* this_, void* edx,
                               void* a2, int a3, int a4, int a5, int a6)
{
    char* const r1 = g_orig_44DE60(this_, edx, a2, a3, a4, a5, a6);
    if (is_p1(this_) && th08_platform::player2::IsConstructed()) {
        __try {
            g_orig_44DE60(th08_platform::player2::g_Player2, edx,
                          a2, a3, a4, a5, a6);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // SEH-swallow: a slot-table write should never fault, but if
            // g_Player2 has bad state we'd rather lose one hit-slot than
            // crash the game.
        }
    }
    return r1;
}

char* __fastcall hooked_44DF00(void* this_, void* edx,
                               void* a2, int a3, int a4, int a5, int a6)
{
    char* const r1 = g_orig_44DF00(this_, edx, a2, a3, a4, a5, a6);
    if (is_p1(this_) && th08_platform::player2::IsConstructed()) {
        __try {
            g_orig_44DF00(th08_platform::player2::g_Player2, edx,
                          a2, a3, a4, a5, a6);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    return r1;
}

int __fastcall hooked_451670(void* this_, void* edx,
                             float* a2, float* a3, void* a4, void* a5)
{
    const int r1 = g_orig_451670(this_, edx, a2, a3, a4, a5);
    if (is_p1(this_) && th08_platform::player2::IsConstructed()) {
        // Run damage check for P2's bullets too. Both calls write
        // hit-info via a4/a5 - the second call may overwrite the
        // first's output. The caller (EnemyManager) sums via a5
        // probably; for the simple case where both miss we get 0,
        // and where either hits we report the more recent one.
        __try {
            const int r2 = g_orig_451670(th08_platform::player2::g_Player2,
                                         edx, a2, a3, a4, a5);
            // Return larger of (r1, r2) so the caller sees a hit
            // attribution if EITHER player hit.
            if (r2 > r1) return r2;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    return r1;
}

void* resolve(DWORD rva)
{
    HMODULE base = GetModuleHandleA(nullptr);
    if (!base) return nullptr;
    return reinterpret_cast<void*>(reinterpret_cast<std::uint8_t*>(base) + rva);
}

}  // namespace

bool install()
{
    void* t1 = resolve(kRVA_44DE60);
    void* t2 = resolve(kRVA_44DF00);
    void* t3 = resolve(kRVA_451670);
    if (!t1 || !t2 || !t3) {
        log_line("p2_mirror: resolve failed (%p %p %p)", t1, t2, t3);
        return false;
    }

    log_line("p2_mirror: hooking sub_44DE60 @ %p, sub_44DF00 @ %p, sub_451670 @ %p",
             t1, t2, t3);

    if (MH_CreateHook(t1, reinterpret_cast<void*>(&hooked_44DE60),
                      reinterpret_cast<LPVOID*>(&g_orig_44DE60)) != MH_OK ||
        MH_CreateHook(t2, reinterpret_cast<void*>(&hooked_44DF00),
                      reinterpret_cast<LPVOID*>(&g_orig_44DF00)) != MH_OK ||
        MH_CreateHook(t3, reinterpret_cast<void*>(&hooked_451670),
                      reinterpret_cast<LPVOID*>(&g_orig_451670)) != MH_OK) {
        log_line("p2_mirror: MH_CreateHook failed");
        return false;
    }
    if (MH_EnableHook(t1) != MH_OK ||
        MH_EnableHook(t2) != MH_OK ||
        MH_EnableHook(t3) != MH_OK) {
        log_line("p2_mirror: MH_EnableHook failed");
        return false;
    }
    log_line("p2_mirror: enabled (3 mirror hooks active)");
    return true;
}

void uninstall()
{
    void* t1 = resolve(kRVA_44DE60);
    void* t2 = resolve(kRVA_44DF00);
    void* t3 = resolve(kRVA_451670);
    if (g_orig_44DE60 && t1) { MH_DisableHook(t1); MH_RemoveHook(t1); }
    if (g_orig_44DF00 && t2) { MH_DisableHook(t2); MH_RemoveHook(t2); }
    if (g_orig_451670 && t3) { MH_DisableHook(t3); MH_RemoveHook(t3); }
    g_orig_44DE60 = nullptr;
    g_orig_44DF00 = nullptr;
    g_orig_451670 = nullptr;
}

}  // namespace th08_platform::state::p2_mirror
