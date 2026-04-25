#include "item_routing.h"

#include <windows.h>
#include <MinHook.h>

#include <atomic>
#include <cstdint>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "../logging.h"
#include "globals.h"
#include "player2.h"
#include "player_layout.h"

namespace th08_platform::state::item_routing {

namespace {

// Worksheet Section 2 / IDA verified this turn:
//   Player::CalcItemBoxCollision @ 0x44A5A0
//     __thiscall(Player* this, float item_pos[3], int meta) -> BOOL
//   sub_44C1B0 @ 0x44C1B0
//     __thiscall(Player* this, float item_pos[3]) -> angle in ST0
//   ItemManager::OnUpdate item-attraction call site:
//     0x440881: ECX=&g_Player
//     0x440886: call sub_44C1B0
//     0x44088B: return address after call
constexpr DWORD kCalcItemBoxCollisionRVA = 0x44A5A0 - 0x400000;
constexpr DWORD kAngleToPlayerRVA = 0x44C1B0 - 0x400000;
constexpr DWORD kItemAttractReturnRVA = 0x44088B - 0x400000;

using CalcItemBoxCollision_t = int(__fastcall*)(void* this_, void* edx_unused,
                                                float* item_pos, int meta);
using AngleToPlayer_t = double(__fastcall*)(void* this_, void* edx_unused,
                                            float* item_pos);

CalcItemBoxCollision_t g_orig_calc_item_box_collision = nullptr;
AngleToPlayer_t g_orig_angle_to_player = nullptr;

std::atomic<unsigned long long> g_items_routed_calls{0};
std::atomic<unsigned long long> g_items_routed_p1{0};
std::atomic<unsigned long long> g_items_routed_p2{0};
std::atomic<unsigned long long> g_items_attracted_p1{0};
std::atomic<unsigned long long> g_items_attracted_p2{0};

void* resolve(DWORD rva)
{
    HMODULE base = GetModuleHandleA(nullptr);
    if (!base) return nullptr;
    return reinterpret_cast<void*>(reinterpret_cast<std::uint8_t*>(base) + rva);
}

inline void* p1() noexcept
{
    return reinterpret_cast<void*>(globals::kAddr_g_Player);
}

inline void* p2() noexcept
{
    return th08_platform::player2::g_Player2;
}

inline bool is_p1(void* player) noexcept
{
    return player == p1();
}

bool is_p1_eligible() noexcept
{
    const auto* const player = reinterpret_cast<const std::uint8_t*>(globals::kAddr_g_Player);
    const std::uint8_t state =
        player[player_layout::kOff_playerState];
    return state != 2 && state != 3;
}

float player_x(void* player) noexcept
{
    auto* const bytes = reinterpret_cast<std::uint8_t*>(player);
    return *reinterpret_cast<float*>(bytes + player_layout::kOff_pos_x);
}

float player_y(void* player) noexcept
{
    auto* const bytes = reinterpret_cast<std::uint8_t*>(player);
    return *reinterpret_cast<float*>(bytes + player_layout::kOff_pos_y);
}

float dist2(float* item_pos, void* player) noexcept
{
    const float dx = item_pos[0] - player_x(player);
    const float dy = item_pos[1] - player_y(player);
    return dx * dx + dy * dy;
}

struct Selection {
    void* player;
    bool is_p2;
};

Selection select_nearest_eligible(float* item_pos) noexcept
{
    const bool p1_ok = is_p1_eligible();
    const bool p2_ok = th08_platform::player2::IsEligible();

    if (p1_ok && !p2_ok) return {p1(), false};
    if (!p1_ok && p2_ok) return {p2(), true};
    if (!p1_ok && !p2_ok) return {p1(), false};
    if (!item_pos) return {p1(), false};

    return dist2(item_pos, p2()) < dist2(item_pos, p1())
        ? Selection{p2(), true}
        : Selection{p1(), false};
}

int __fastcall hooked_CalcItemBoxCollision(void* this_, void* edx,
                                           float* item_pos, int meta)
{
    g_items_routed_calls.fetch_add(1, std::memory_order_relaxed);

    if (!is_p1(this_)) {
        return g_orig_calc_item_box_collision(this_, edx, item_pos, meta);
    }

    const Selection selected = select_nearest_eligible(item_pos);
    const int result =
        g_orig_calc_item_box_collision(selected.player, edx, item_pos, meta);

    // Count actual successful item-predicate attribution. Failed predicate
    // calls still contribute to routed_calls so we can tell the detour is hot.
    if (result) {
        auto& counter = selected.is_p2 ? g_items_routed_p2 : g_items_routed_p1;
        counter.fetch_add(1, std::memory_order_relaxed);
    }
    return result;
}

double __fastcall hooked_AngleToPlayer(void* this_, void* edx, float* item_pos)
{
    void* return_address = nullptr;
#if defined(_MSC_VER)
    return_address = _ReturnAddress();
#elif defined(__GNUC__) || defined(__clang__)
    return_address = __builtin_return_address(0);
#endif

    if (!is_p1(this_) || return_address != resolve(kItemAttractReturnRVA)) {
        return g_orig_angle_to_player(this_, edx, item_pos);
    }

    const Selection selected = select_nearest_eligible(item_pos);
    auto& counter = selected.is_p2 ? g_items_attracted_p2 : g_items_attracted_p1;
    counter.fetch_add(1, std::memory_order_relaxed);
    return g_orig_angle_to_player(selected.player, edx, item_pos);
}

void cleanup_hooks(void* t_collision, void* t_angle) noexcept
{
    if (g_orig_calc_item_box_collision && t_collision) {
        MH_DisableHook(t_collision);
        MH_RemoveHook(t_collision);
    }
    if (g_orig_angle_to_player && t_angle) {
        MH_DisableHook(t_angle);
        MH_RemoveHook(t_angle);
    }
    g_orig_calc_item_box_collision = nullptr;
    g_orig_angle_to_player = nullptr;
}

}  // namespace

bool install()
{
    if (g_orig_calc_item_box_collision || g_orig_angle_to_player) {
        log_line("item_routing: already installed");
        return true;
    }

    void* const t_collision = resolve(kCalcItemBoxCollisionRVA);
    void* const t_angle = resolve(kAngleToPlayerRVA);
    if (!t_collision || !t_angle) {
        log_line("item_routing: resolve failed (collision=%p angle=%p)",
                 t_collision, t_angle);
        return false;
    }

    log_line("item_routing: hooking CalcItemBoxCollision @ %p and sub_44C1B0 @ %p",
             t_collision, t_angle);

    MH_STATUS status = MH_CreateHook(
        t_collision, reinterpret_cast<void*>(&hooked_CalcItemBoxCollision),
        reinterpret_cast<LPVOID*>(&g_orig_calc_item_box_collision));
    if (status != MH_OK) {
        log_line("item_routing: MH_CreateHook CalcItemBoxCollision failed status=%d",
                 status);
        return false;
    }

    status = MH_CreateHook(t_angle, reinterpret_cast<void*>(&hooked_AngleToPlayer),
                           reinterpret_cast<LPVOID*>(&g_orig_angle_to_player));
    if (status != MH_OK) {
        log_line("item_routing: MH_CreateHook sub_44C1B0 failed status=%d", status);
        cleanup_hooks(t_collision, t_angle);
        return false;
    }

    status = MH_EnableHook(t_collision);
    if (status != MH_OK) {
        log_line("item_routing: MH_EnableHook CalcItemBoxCollision failed status=%d",
                 status);
        cleanup_hooks(t_collision, t_angle);
        return false;
    }

    status = MH_EnableHook(t_angle);
    if (status != MH_OK) {
        log_line("item_routing: MH_EnableHook sub_44C1B0 failed status=%d", status);
        cleanup_hooks(t_collision, t_angle);
        return false;
    }

    log_line("item_routing: enabled (collection + attraction direction)");
    return true;
}

void uninstall()
{
    cleanup_hooks(resolve(kCalcItemBoxCollisionRVA), resolve(kAngleToPlayerRVA));
}

Counters snapshot_counters()
{
    Counters c;
    c.routed_calls = g_items_routed_calls.load(std::memory_order_relaxed);
    c.routed_p1 = g_items_routed_p1.load(std::memory_order_relaxed);
    c.routed_p2 = g_items_routed_p2.load(std::memory_order_relaxed);
    c.attracted_p1 = g_items_attracted_p1.load(std::memory_order_relaxed);
    c.attracted_p2 = g_items_attracted_p2.load(std::memory_order_relaxed);
    return c;
}

}  // namespace th08_platform::state::item_routing
