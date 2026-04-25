#include "item_routing.h"

#include <windows.h>
#include <MinHook.h>

#include <atomic>
#include <cstddef>
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
//   ItemManager::OnUpdate auto-attract trigger:
//     0x440801: fld g_Player.y
//     0x440807: compare against *(dword_18B896C + 0x1C)
//     0x44085F..0x440875: read g_Player.playerState before state-1 transition
constexpr DWORD kCalcItemBoxCollisionRVA = 0x44A5A0 - 0x400000;
constexpr DWORD kAngleToPlayerRVA = 0x44C1B0 - 0x400000;
constexpr DWORD kItemManagerOnUpdateRVA = 0x440500 - 0x400000;
constexpr DWORD kItemAttractReturnRVA = 0x44088B - 0x400000;
constexpr std::uintptr_t kAddr_g_CurrentPlayerData = 0x018B896C;
constexpr std::size_t kOff_playerModeFlag = 0x0003;  // byte_17D5EFB reads in OnUpdate.
constexpr std::size_t kOff_playerDataAutoCollectLine = 0x001C;
constexpr std::size_t kOff_itemManagerActiveHead = 0x17B088;
constexpr std::size_t kOff_itemCurrentPos = 0x02A4;
constexpr std::size_t kOff_itemState = 0x02D7;
constexpr std::size_t kOff_itemNext = 0x02DC;
constexpr unsigned kMaxItemTriggerScan = 4096;

using CalcItemBoxCollision_t = int(__fastcall*)(void* this_, void* edx_unused,
                                                float* item_pos, int meta);
using AngleToPlayer_t = double(__fastcall*)(void* this_, void* edx_unused,
                                            float* item_pos);
using ItemManagerOnUpdate_t = int(__fastcall*)(void* this_, void* edx_unused);

CalcItemBoxCollision_t g_orig_calc_item_box_collision = nullptr;
AngleToPlayer_t g_orig_angle_to_player = nullptr;
ItemManagerOnUpdate_t g_orig_item_manager_on_update = nullptr;

std::atomic<unsigned long long> g_items_routed_calls{0};
std::atomic<unsigned long long> g_items_routed_p1{0};
std::atomic<unsigned long long> g_items_routed_p2{0};
std::atomic<unsigned long long> g_items_attracted_p1{0};
std::atomic<unsigned long long> g_items_attracted_p2{0};
std::atomic<unsigned long long> g_attract_trigger_p1{0};
std::atomic<unsigned long long> g_attract_trigger_p2{0};

struct P1SpoofContext {
    bool active;
    std::uint8_t saved_state;
    std::uint8_t saved_mode_flag;
    float saved_y;
};

thread_local P1SpoofContext g_p1_spoof{};

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

std::uint8_t* p1_bytes() noexcept
{
    return reinterpret_cast<std::uint8_t*>(globals::kAddr_g_Player);
}

float* p1_y_ptr() noexcept
{
    return reinterpret_cast<float*>(p1_bytes() + player_layout::kOff_pos_y);
}

std::uint8_t player_state(void* player) noexcept
{
    if (is_p1(player) && g_p1_spoof.active) {
        return g_p1_spoof.saved_state;
    }
    const auto* const bytes = reinterpret_cast<const std::uint8_t*>(player);
    return bytes[player_layout::kOff_playerState];
}

std::uint8_t player_mode_flag(void* player) noexcept
{
    if (is_p1(player) && g_p1_spoof.active) {
        return g_p1_spoof.saved_mode_flag;
    }
    const auto* const bytes = reinterpret_cast<const std::uint8_t*>(player);
    return bytes[kOff_playerModeFlag];
}

bool is_p1_eligible() noexcept
{
    const std::uint8_t state = player_state(p1());
    return state != 2 && state != 3;
}

float player_x(void* player) noexcept
{
    auto* const bytes = reinterpret_cast<std::uint8_t*>(player);
    return *reinterpret_cast<float*>(bytes + player_layout::kOff_pos_x);
}

float player_y(void* player) noexcept
{
    if (is_p1(player) && g_p1_spoof.active) {
        return g_p1_spoof.saved_y;
    }
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

bool env_flag(const char* name, bool default_value) noexcept
{
    char buf[8] = {};
    const DWORD len = GetEnvironmentVariableA(name, buf,
                                              static_cast<DWORD>(sizeof(buf)));
    if (len == 0) return default_value;
    return buf[0] != '0';
}

bool read_auto_collect_line(float& line_y) noexcept
{
    const auto player_data =
        *reinterpret_cast<std::uintptr_t*>(kAddr_g_CurrentPlayerData);
    if (!player_data) return false;

    line_y = *reinterpret_cast<float*>(
        player_data + kOff_playerDataAutoCollectLine);
    return true;
}

bool can_trigger_auto_attract(void* player, float line_y) noexcept
{
    const std::uint8_t state = player_state(player);
    return state != 1 && state != 2 && state != 3 &&
           player_y(player) < line_y;
}

Selection select_nearest_freefall_item_owner(void* manager) noexcept
{
    if (!manager) return {p1(), false};

    auto* const bytes = reinterpret_cast<std::uint8_t*>(manager);
    std::uintptr_t item =
        *reinterpret_cast<std::uintptr_t*>(bytes + kOff_itemManagerActiveHead);

    unsigned p1_votes = 0;
    unsigned p2_votes = 0;
    for (unsigned scanned = 0; item && scanned < kMaxItemTriggerScan; ++scanned) {
        auto* const item_bytes = reinterpret_cast<std::uint8_t*>(item);
        const std::uint8_t item_state = item_bytes[kOff_itemState];
        if (item_state == 0) {
            auto* const item_pos =
                reinterpret_cast<float*>(item_bytes + kOff_itemCurrentPos);
            if (dist2(item_pos, p2()) < dist2(item_pos, p1())) {
                ++p2_votes;
            } else {
                ++p1_votes;
            }
        }
        item = *reinterpret_cast<std::uintptr_t*>(item_bytes + kOff_itemNext);
    }

    return p2_votes > p1_votes ? Selection{p2(), true} : Selection{p1(), false};
}

Selection select_attract_trigger_target(void* manager) noexcept
{
    float line_y = 0.0f;
    if (!read_auto_collect_line(line_y)) return {p1(), false};

    const bool p1_ok = can_trigger_auto_attract(p1(), line_y);
    const bool p2_ok = th08_platform::player2::IsEligible() &&
        can_trigger_auto_attract(p2(), line_y);

    if (p1_ok && !p2_ok) return {p1(), false};
    if (!p1_ok && p2_ok) return {p2(), true};
    if (p1_ok && p2_ok) return select_nearest_freefall_item_owner(manager);

    return {p1(), false};
}

void begin_p1_spoof(const Selection& selected) noexcept
{
    auto* const player = p1_bytes();
    g_p1_spoof.saved_state = player[player_layout::kOff_playerState];
    g_p1_spoof.saved_mode_flag = player[kOff_playerModeFlag];
    g_p1_spoof.saved_y = *p1_y_ptr();
    g_p1_spoof.active = true;

    player[player_layout::kOff_playerState] = player_state(selected.player);
    player[kOff_playerModeFlag] = player_mode_flag(selected.player);
    *p1_y_ptr() = player_y(selected.player);
}

void restore_p1_spoof() noexcept
{
    if (!g_p1_spoof.active) return;

    auto* const player = p1_bytes();
    player[player_layout::kOff_playerState] = g_p1_spoof.saved_state;
    player[kOff_playerModeFlag] = g_p1_spoof.saved_mode_flag;
    *p1_y_ptr() = g_p1_spoof.saved_y;
    g_p1_spoof.active = false;
}

class TemporaryRealP1Fields {
public:
    explicit TemporaryRealP1Fields(bool enable) noexcept
        : armed_(enable && g_p1_spoof.active)
    {
        if (!armed_) return;

        auto* const player = p1_bytes();
        spoof_state_ = player[player_layout::kOff_playerState];
        spoof_mode_flag_ = player[kOff_playerModeFlag];
        spoof_y_ = *p1_y_ptr();

        player[player_layout::kOff_playerState] = g_p1_spoof.saved_state;
        player[kOff_playerModeFlag] = g_p1_spoof.saved_mode_flag;
        *p1_y_ptr() = g_p1_spoof.saved_y;
    }

    ~TemporaryRealP1Fields() noexcept
    {
        if (!armed_) return;

        auto* const player = p1_bytes();
        player[player_layout::kOff_playerState] = spoof_state_;
        player[kOff_playerModeFlag] = spoof_mode_flag_;
        *p1_y_ptr() = spoof_y_;
    }

private:
    bool armed_ = false;
    std::uint8_t spoof_state_ = 0;
    std::uint8_t spoof_mode_flag_ = 0;
    float spoof_y_ = 0.0f;
};

int __fastcall hooked_CalcItemBoxCollision(void* this_, void* edx,
                                           float* item_pos, int meta)
{
    g_items_routed_calls.fetch_add(1, std::memory_order_relaxed);

    if (!is_p1(this_)) {
        return g_orig_calc_item_box_collision(this_, edx, item_pos, meta);
    }

    const Selection selected = select_nearest_eligible(item_pos);
    const TemporaryRealP1Fields real_p1_fields(selected.player == p1());
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

    const TemporaryRealP1Fields real_p1_fields(selected.player == p1());
    return g_orig_angle_to_player(selected.player, edx, item_pos);
}

int __fastcall hooked_ItemManagerOnUpdate(void* this_, void* edx)
{
    if (!g_orig_item_manager_on_update) return 0;
    if (g_p1_spoof.active) {
        return g_orig_item_manager_on_update(this_, edx);
    }

    const Selection selected = select_attract_trigger_target(this_);
    auto& counter = selected.is_p2 ? g_attract_trigger_p2 : g_attract_trigger_p1;
    counter.fetch_add(1, std::memory_order_relaxed);

    begin_p1_spoof(selected);
    int result = 0;

#if defined(_MSC_VER)
    __try {
        result = g_orig_item_manager_on_update(this_, edx);
    } __finally {
        restore_p1_spoof();
    }
#else
    try {
        result = g_orig_item_manager_on_update(this_, edx);
    } catch (...) {
        restore_p1_spoof();
        throw;
    }
    restore_p1_spoof();
#endif

    return result;
}

void cleanup_hooks(void* t_collision, void* t_angle, void* t_update) noexcept
{
    if (g_orig_calc_item_box_collision && t_collision) {
        MH_DisableHook(t_collision);
        MH_RemoveHook(t_collision);
    }
    if (g_orig_angle_to_player && t_angle) {
        MH_DisableHook(t_angle);
        MH_RemoveHook(t_angle);
    }
    if (g_orig_item_manager_on_update && t_update) {
        MH_DisableHook(t_update);
        MH_RemoveHook(t_update);
    }
    g_orig_calc_item_box_collision = nullptr;
    g_orig_angle_to_player = nullptr;
    g_orig_item_manager_on_update = nullptr;
}

}  // namespace

bool install()
{
    if (g_orig_calc_item_box_collision || g_orig_angle_to_player ||
        g_orig_item_manager_on_update) {
        log_line("item_routing: already installed");
        return true;
    }

    const bool attract_trigger_enabled =
        env_flag("TH08_PLATFORM_ATTRACT_TRIGGER", true);
    void* const t_collision = resolve(kCalcItemBoxCollisionRVA);
    void* const t_angle = resolve(kAngleToPlayerRVA);
    void* const t_update =
        attract_trigger_enabled ? resolve(kItemManagerOnUpdateRVA) : nullptr;
    if (!t_collision || !t_angle || (attract_trigger_enabled && !t_update)) {
        log_line("item_routing: resolve failed (collision=%p angle=%p update=%p)",
                 t_collision, t_angle, t_update);
        return false;
    }

    if (attract_trigger_enabled) {
        log_line("item_routing: hooking CalcItemBoxCollision @ %p, sub_44C1B0 @ %p, ItemManager::OnUpdate @ %p",
                 t_collision, t_angle, t_update);
    } else {
        log_line("item_routing: hooking CalcItemBoxCollision @ %p and sub_44C1B0 @ %p (trigger disabled)",
                 t_collision, t_angle);
    }

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
        cleanup_hooks(t_collision, t_angle, t_update);
        return false;
    }

    if (attract_trigger_enabled) {
        status = MH_CreateHook(
            t_update, reinterpret_cast<void*>(&hooked_ItemManagerOnUpdate),
            reinterpret_cast<LPVOID*>(&g_orig_item_manager_on_update));
        if (status != MH_OK) {
            log_line("item_routing: MH_CreateHook ItemManager::OnUpdate failed status=%d",
                     status);
            cleanup_hooks(t_collision, t_angle, t_update);
            return false;
        }
    }

    status = MH_EnableHook(t_collision);
    if (status != MH_OK) {
        log_line("item_routing: MH_EnableHook CalcItemBoxCollision failed status=%d",
                 status);
        cleanup_hooks(t_collision, t_angle, t_update);
        return false;
    }

    status = MH_EnableHook(t_angle);
    if (status != MH_OK) {
        log_line("item_routing: MH_EnableHook sub_44C1B0 failed status=%d", status);
        cleanup_hooks(t_collision, t_angle, t_update);
        return false;
    }

    if (attract_trigger_enabled) {
        status = MH_EnableHook(t_update);
        if (status != MH_OK) {
            log_line("item_routing: MH_EnableHook ItemManager::OnUpdate failed status=%d",
                     status);
            cleanup_hooks(t_collision, t_angle, t_update);
            return false;
        }
    }

    log_line("item_routing: enabled (collection + attraction direction%s)",
             attract_trigger_enabled ? " + trigger" : "");
    return true;
}

void uninstall()
{
    cleanup_hooks(resolve(kCalcItemBoxCollisionRVA), resolve(kAngleToPlayerRVA),
                  resolve(kItemManagerOnUpdateRVA));
}

Counters snapshot_counters()
{
    Counters c;
    c.routed_calls = g_items_routed_calls.load(std::memory_order_relaxed);
    c.routed_p1 = g_items_routed_p1.load(std::memory_order_relaxed);
    c.routed_p2 = g_items_routed_p2.load(std::memory_order_relaxed);
    c.attracted_p1 = g_items_attracted_p1.load(std::memory_order_relaxed);
    c.attracted_p2 = g_items_attracted_p2.load(std::memory_order_relaxed);
    c.trigger_p1 = g_attract_trigger_p1.load(std::memory_order_relaxed);
    c.trigger_p2 = g_attract_trigger_p2.load(std::memory_order_relaxed);
    return c;
}

}  // namespace th08_platform::state::item_routing
