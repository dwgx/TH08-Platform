#include "enemy_aim.h"

#include <windows.h>
#include <MinHook.h>

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <cstdint>

#include "../hooks/game_loop.h"
#include "../logging.h"
#include "globals.h"
#include "player2.h"
#include "player_layout.h"

#if !defined(_MSC_VER) || !defined(_M_IX86)
#error enemy_aim requires MSVC x86 for inline asm usercall bridges.
#endif

namespace th08_platform::state::enemy_aim {

namespace {

constexpr DWORD kFloatResolverRVA = 0x420120 - 0x400000;
constexpr DWORD kPointerResolverRVA = 0x420950 - 0x400000;
constexpr DWORD kSecondaryResolverRVA = 0x41F420 - 0x400000;
constexpr DWORD kBounceHelperRVA = 0x422020 - 0x400000;
constexpr DWORD kWrapHelperRVA = 0x4224A0 - 0x400000;
constexpr DWORD kSpawnGateRVA = 0x422720 - 0x400000;
constexpr DWORD kVectorHelperRVA = 0x428310 - 0x400000;
constexpr DWORD kEnemyManagerOnUpdateRVA = 0x42C660 - 0x400000;
constexpr DWORD kSpellcardOnUpdateRVA = 0x416B90 - 0x400000;
constexpr DWORD kTargetAcquireBlockRVA = 0x426C40 - 0x400000;

constexpr unsigned kVarPlayerX = 0x273D;
constexpr unsigned kVarPlayerY = 0x273E;
constexpr unsigned kVarPlayerZ = 0x273F;
constexpr unsigned kVarAngleToPlayer = 0x2740;
constexpr unsigned kVarDistanceToPlayer = 0x2742;
constexpr unsigned kVarPlayerPersona = 0x2771;

constexpr std::size_t kOff_enemyPosX = 11572;
constexpr std::size_t kOff_enemyPosY = 11576;
constexpr std::size_t kOff_enemyCalcPosX = 11656;
constexpr std::size_t kOff_enemyCalcPosY = 11660;

using FloatResolver_t = double(__fastcall*)(int this_, void* edx_unused, float code);
using PointerResolver_t = float*(__fastcall*)(int a1, float* a2, unsigned short a3, int a4);
using BounceHelper_t = int(__fastcall*)(int a1, int a2);
using SpawnGate_t = void(__fastcall*)(int a1, int a2);
using VectorHelper_t = int(__fastcall*)(float* a1, float* a2);
using TargetAcquireBlock_t = int(__fastcall*)(void* this_, void* edx_unused);

FloatResolver_t g_orig_float_resolver = nullptr;
PointerResolver_t g_orig_pointer_resolver = nullptr;
void* g_orig_secondary_resolver = nullptr;
BounceHelper_t g_orig_bounce_helper = nullptr;
void* g_orig_wrap_helper = nullptr;
SpawnGate_t g_orig_spawn_gate = nullptr;
VectorHelper_t g_orig_vector_helper = nullptr;
void* g_orig_enemy_manager_on_update = nullptr;
void* g_orig_spellcard_on_update = nullptr;
TargetAcquireBlock_t g_orig_target_acquire_block = nullptr;

enum class HookSlot : std::size_t {
    kFloatResolver = 0,
    kPointerResolver,
    kSecondaryResolver,
    kBounceHelper,
    kWrapHelper,
    kSpawnGate,
    kVectorHelper,
    kEnemyManager,
    kSpellcard,
    kTableBlock,
    kCount,
};

constexpr const char* kHookNames[static_cast<std::size_t>(HookSlot::kCount)] = {
    "FVAR", "PVAR", "SEC", "BNC", "WRP", "GATE", "VEC", "ENM", "SPC", "TBL",
};

struct HookCounterState {
    std::atomic<unsigned long long> p1{0};
    std::atomic<unsigned long long> p2{0};
    std::atomic<long> log_remaining{0};
};

std::atomic<unsigned long long> g_aim_calls_total{0};
HookCounterState g_hook_counters[static_cast<std::size_t>(HookSlot::kCount)];

struct AimTarget {
    void* player = reinterpret_cast<void*>(globals::kAddr_g_Player);
    bool is_p2 = false;
    float pos_x = 0.0f;
    float pos_y = 0.0f;
    float pos_z = 0.0f;
    std::uint8_t player_state = 0;
};

struct SourcePos {
    bool valid = false;
    float x = 0.0f;
    float y = 0.0f;
};

struct AimSpoofContext {
    bool active = false;
    bool spoof_state = false;
    float saved_x = 0.0f;
    float saved_y = 0.0f;
    float saved_z = 0.0f;
    std::uint8_t saved_state = 0;
};

// H2 returns a pointer operand into ECL evaluation. Current xrefs are
// EclManager::RunEcl plus three small ECL helpers (0x421120/0x421180/0x421300);
// the pointer is consumed inside that evaluation step and not retained by any
// caller inspected in this batch. If future RE finds a persistent store, move
// the routing to that caller instead of returning this scratch.
struct PointerScratch {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

thread_local AimSpoofContext g_aim_spoof{};
thread_local PointerScratch g_pointer_scratch{};

void* resolve(DWORD rva) noexcept
{
    HMODULE base = GetModuleHandleA(nullptr);
    if (!base) return nullptr;
    return reinterpret_cast<void*>(reinterpret_cast<std::uint8_t*>(base) + rva);
}

bool env_flag(const char* name, bool default_value) noexcept
{
    char buf[16] = {};
    const DWORD len = GetEnvironmentVariableA(name, buf,
                                              static_cast<DWORD>(sizeof(buf)));
    if (len == 0) return default_value;
    return buf[0] != '0';
}

unsigned env_u32(const char* name, unsigned default_value) noexcept
{
    char buf[32] = {};
    const DWORD len = GetEnvironmentVariableA(name, buf,
                                              static_cast<DWORD>(sizeof(buf)));
    if (len == 0) return default_value;
    char* end = nullptr;
    const unsigned long value = std::strtoul(buf, &end, 10);
    if (!end || *end != '\0') return default_value;
    return static_cast<unsigned>(value);
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

float* p1_x_ptr() noexcept
{
    return reinterpret_cast<float*>(p1_bytes() + player_layout::kOff_pos_x);
}

float* p1_y_ptr() noexcept
{
    return reinterpret_cast<float*>(p1_bytes() + player_layout::kOff_pos_y);
}

float* p1_z_ptr() noexcept
{
    return reinterpret_cast<float*>(p1_bytes() + player_layout::kOff_pos_z);
}

std::uint8_t real_p1_state() noexcept
{
    if (g_aim_spoof.active && g_aim_spoof.spoof_state) {
        return g_aim_spoof.saved_state;
    }
    return p1_bytes()[player_layout::kOff_playerState];
}

float real_p1_x() noexcept
{
    return g_aim_spoof.active ? g_aim_spoof.saved_x : *p1_x_ptr();
}

float real_p1_y() noexcept
{
    return g_aim_spoof.active ? g_aim_spoof.saved_y : *p1_y_ptr();
}

float real_p1_z() noexcept
{
    return g_aim_spoof.active ? g_aim_spoof.saved_z : *p1_z_ptr();
}

std::uint8_t player_state(void* player) noexcept
{
    if (is_p1(player)) {
        return real_p1_state();
    }
    const auto* const bytes = reinterpret_cast<const std::uint8_t*>(player);
    return bytes[player_layout::kOff_playerState];
}

float player_x(void* player) noexcept
{
    if (is_p1(player)) {
        return real_p1_x();
    }
    const auto* const bytes = reinterpret_cast<const std::uint8_t*>(player);
    return *reinterpret_cast<const float*>(bytes + player_layout::kOff_pos_x);
}

float player_y(void* player) noexcept
{
    if (is_p1(player)) {
        return real_p1_y();
    }
    const auto* const bytes = reinterpret_cast<const std::uint8_t*>(player);
    return *reinterpret_cast<const float*>(bytes + player_layout::kOff_pos_y);
}

float player_z(void* player) noexcept
{
    if (is_p1(player)) {
        return real_p1_z();
    }
    const auto* const bytes = reinterpret_cast<const std::uint8_t*>(player);
    return *reinterpret_cast<const float*>(bytes + player_layout::kOff_pos_z);
}

bool is_p1_eligible() noexcept
{
    const std::uint8_t state = real_p1_state();
    return state != 2 && state != 3;
}

AimTarget make_target(void* player, bool is_p2_target) noexcept
{
    AimTarget target;
    target.player = player;
    target.is_p2 = is_p2_target;
    target.pos_x = player_x(player);
    target.pos_y = player_y(player);
    target.pos_z = player_z(player);
    target.player_state = player_state(player);
    return target;
}

float dist2(const SourcePos& source, const AimTarget& target) noexcept
{
    const float dx = source.x - target.pos_x;
    const float dy = source.y - target.pos_y;
    return dx * dx + dy * dy;
}

AimTarget select_nearest_eligible_aim_target(const SourcePos* source = nullptr) noexcept
{
    const bool p1_ok = is_p1_eligible();
    const bool p2_ok = th08_platform::player2::IsEligible();

    if (p1_ok && !p2_ok) return make_target(p1(), false);
    if (!p1_ok && p2_ok) return make_target(p2(), true);
    if (!p1_ok && !p2_ok) return make_target(p1(), false);

    const AimTarget p1_target = make_target(p1(), false);
    const AimTarget p2_target = make_target(p2(), true);

    if (source && source->valid) {
        return dist2(*source, p2_target) < dist2(*source, p1_target)
            ? p2_target
            : p1_target;
    }

    const std::uint64_t frame = th08_platform::hooks::current_frame();
    return (frame != 0 && (frame & 1ull) != 0) ? p2_target : p1_target;
}

void begin_p1_spoof(const AimTarget& target, bool spoof_state) noexcept
{
    auto* const player = p1_bytes();
    g_aim_spoof.saved_x = *p1_x_ptr();
    g_aim_spoof.saved_y = *p1_y_ptr();
    g_aim_spoof.saved_z = *p1_z_ptr();
    g_aim_spoof.saved_state = player[player_layout::kOff_playerState];
    g_aim_spoof.spoof_state = spoof_state;
    g_aim_spoof.active = true;

    *p1_x_ptr() = target.pos_x;
    *p1_y_ptr() = target.pos_y;
    *p1_z_ptr() = target.pos_z;
    if (spoof_state) {
        player[player_layout::kOff_playerState] = target.player_state;
    }
}

void restore_p1_spoof() noexcept
{
    if (!g_aim_spoof.active) return;

    auto* const player = p1_bytes();
    *p1_x_ptr() = g_aim_spoof.saved_x;
    *p1_y_ptr() = g_aim_spoof.saved_y;
    *p1_z_ptr() = g_aim_spoof.saved_z;
    if (g_aim_spoof.spoof_state) {
        player[player_layout::kOff_playerState] = g_aim_spoof.saved_state;
    }

    g_aim_spoof.active = false;
    g_aim_spoof.spoof_state = false;
}

void maybe_log_decision(HookSlot slot, const AimTarget& target,
                        const SourcePos* source) noexcept
{
    auto& remaining =
        g_hook_counters[static_cast<std::size_t>(slot)].log_remaining;
    long current = remaining.load(std::memory_order_relaxed);
    while (current > 0) {
        if (remaining.compare_exchange_weak(current, current - 1,
                                            std::memory_order_relaxed,
                                            std::memory_order_relaxed)) {
            if (source && source->valid) {
                th08_platform::log_line(
                    "enemy_aim:%s choose=%s src=(%.1f,%.1f) target=(%.1f,%.1f) frame=%llu",
                    kHookNames[static_cast<std::size_t>(slot)],
                    target.is_p2 ? "P2" : "P1",
                    source->x, source->y, target.pos_x, target.pos_y,
                    static_cast<unsigned long long>(th08_platform::hooks::current_frame()));
            } else {
                th08_platform::log_line(
                    "enemy_aim:%s choose=%s src=<round-robin frame=%llu> target=(%.1f,%.1f)",
                    kHookNames[static_cast<std::size_t>(slot)],
                    target.is_p2 ? "P2" : "P1",
                    static_cast<unsigned long long>(th08_platform::hooks::current_frame()),
                    target.pos_x, target.pos_y);
            }
            return;
        }
    }
}

void record_choice(HookSlot slot, const AimTarget& target,
                   const SourcePos* source) noexcept
{
    g_aim_calls_total.fetch_add(1, std::memory_order_relaxed);
    auto& counters = g_hook_counters[static_cast<std::size_t>(slot)];
    auto& bucket = target.is_p2 ? counters.p2 : counters.p1;
    bucket.fetch_add(1, std::memory_order_relaxed);
    maybe_log_decision(slot, target, source);
}

SourcePos source_from_fields(std::uintptr_t base, std::size_t off_x,
                             std::size_t off_y) noexcept
{
    SourcePos source;
    if (base == 0) return source;
    source.valid = true;
    source.x = *reinterpret_cast<const float*>(base + off_x);
    source.y = *reinterpret_cast<const float*>(base + off_y);
    return source;
}

SourcePos resolver_source(int this_, unsigned code) noexcept
{
    switch (code) {
    case kVarAngleToPlayer:
    case kVarDistanceToPlayer:
        return source_from_fields(static_cast<std::uintptr_t>(this_),
                                  kOff_enemyCalcPosX, kOff_enemyCalcPosY);
    default:
        return {};
    }
}

bool is_float_resolver_target(unsigned code) noexcept
{
    switch (code) {
    case kVarPlayerX:
    case kVarPlayerY:
    case kVarPlayerZ:
    case kVarAngleToPlayer:
    case kVarDistanceToPlayer:
    case kVarPlayerPersona:
        return true;
    default:
        return false;
    }
}

bool is_pointer_resolver_target(unsigned code) noexcept
{
    return code == kVarPlayerX || code == kVarPlayerY || code == kVarPlayerZ;
}

bool needs_state_spoof(unsigned code) noexcept
{
    return code == kVarPlayerPersona;
}

double call_orig_float_resolver(int this_, float code) noexcept
{
    return g_orig_float_resolver(this_, nullptr, code);
}

float* call_orig_pointer_resolver(int a1, float* a2, unsigned short a3, int a4) noexcept
{
    return g_orig_pointer_resolver(a1, a2, a3, a4);
}

int call_orig_secondary_resolver(std::uint64_t code, int a2) noexcept
{
    int result = 0;
    const unsigned long lo_value =
        static_cast<unsigned long>(code & 0xFFFFFFFFull);
    const unsigned long hi_value = static_cast<unsigned long>(code >> 32);
    __asm {
        mov eax, lo_value
        mov edx, hi_value
        mov ecx, a2
        call dword ptr [g_orig_secondary_resolver]
        mov result, eax
    }
    return result;
}

int call_orig_bounce_helper(int a1, int a2) noexcept
{
    return g_orig_bounce_helper(a1, a2);
}

int call_orig_wrap_helper(int a1, int a2, double a3) noexcept
{
    int result = 0;
    double arg = a3;
    __asm {
        mov edx, a1
        mov ecx, a2
        fld qword ptr [arg]
        call dword ptr [g_orig_wrap_helper]
        mov result, eax
    }
    return result;
}

void call_orig_spawn_gate(int a1, int a2) noexcept
{
    g_orig_spawn_gate(a1, a2);
}

int call_orig_vector_helper(float* a1, float* a2) noexcept
{
    return g_orig_vector_helper(a1, a2);
}

int call_orig_enemy_manager_on_update(int a1, double a2) noexcept
{
    int result = 0;
    double arg = a2;
    __asm {
        mov ecx, a1
        fld qword ptr [arg]
        call dword ptr [g_orig_enemy_manager_on_update]
        mov result, eax
    }
    return result;
}

int call_orig_spellcard_on_update(int a1, double a2) noexcept
{
    int result = 0;
    double arg = a2;
    __asm {
        mov ecx, a1
        fld qword ptr [arg]
        call dword ptr [g_orig_spellcard_on_update]
        mov result, eax
    }
    return result;
}

int call_orig_target_acquire_block(void* this_) noexcept
{
    return g_orig_target_acquire_block(this_, nullptr);
}

double __fastcall hooked_420120(int this_, void* /*edx_unused*/, float code_as_float)
{
    if (!g_orig_float_resolver) return code_as_float;

    const unsigned code = static_cast<unsigned>(code_as_float);
    if (g_aim_spoof.active || !is_float_resolver_target(code)) {
        return call_orig_float_resolver(this_, code_as_float);
    }

    const SourcePos source = resolver_source(this_, code);
    const SourcePos* source_ptr = source.valid ? &source : nullptr;
    const AimTarget target = select_nearest_eligible_aim_target(source_ptr);
    record_choice(HookSlot::kFloatResolver, target, source_ptr);

    begin_p1_spoof(target, needs_state_spoof(code));
    double result = 0.0;
    __try {
        result = call_orig_float_resolver(this_, code_as_float);
    } __finally {
        restore_p1_spoof();
    }
    return result;
}

float* __fastcall hooked_420950(int a1, float* a2, unsigned short a3, int a4)
{
    if (!g_orig_pointer_resolver || !a2) return a2;
    if (g_aim_spoof.active) {
        return call_orig_pointer_resolver(a1, a2, a3, a4);
    }

    if (a4 >= 32) {
        return call_orig_pointer_resolver(a1, a2, a3, a4);
    }
    if (a4 >= 0 && ((1u << a4) & a3) == 0) {
        return a2;
    }

    const unsigned code = static_cast<unsigned>(*a2);
    if (!is_pointer_resolver_target(code)) {
        return call_orig_pointer_resolver(a1, a2, a3, a4);
    }

    const AimTarget target = select_nearest_eligible_aim_target();
    record_choice(HookSlot::kPointerResolver, target, nullptr);

    g_pointer_scratch.x = target.pos_x;
    g_pointer_scratch.y = target.pos_y;
    g_pointer_scratch.z = target.pos_z;
    switch (code) {
    case kVarPlayerX:
        return &g_pointer_scratch.x;
    case kVarPlayerY:
        return &g_pointer_scratch.y;
    case kVarPlayerZ:
        return &g_pointer_scratch.z;
    default:
        return call_orig_pointer_resolver(a1, a2, a3, a4);
    }
}

extern "C" int __stdcall hooked_41F420_bridge(unsigned int low, unsigned int high,
                                               int a2)
{
    const std::uint64_t code =
        (static_cast<std::uint64_t>(high) << 32) | low;
    const unsigned selector = high;
    if (!g_orig_secondary_resolver) return 0;
    if (g_aim_spoof.active || !is_float_resolver_target(selector)) {
        return call_orig_secondary_resolver(code, a2);
    }

    const SourcePos source = resolver_source(a2, selector);
    const SourcePos* source_ptr = source.valid ? &source : nullptr;
    const AimTarget target = select_nearest_eligible_aim_target(source_ptr);
    record_choice(HookSlot::kSecondaryResolver, target, source_ptr);

    begin_p1_spoof(target, needs_state_spoof(selector));
    int result = 0;
    __try {
        result = call_orig_secondary_resolver(code, a2);
    } __finally {
        restore_p1_spoof();
    }
    return result;
}

extern "C" __declspec(naked) int hooked_41F420()
{
    __asm {
        push ecx
        push edx
        push eax
        call hooked_41F420_bridge
        add esp, 12
        ret
    }
}

int __fastcall hooked_422020(int a1, int a2)
{
    if (!g_orig_bounce_helper) return 0;
    if (g_aim_spoof.active) {
        return call_orig_bounce_helper(a1, a2);
    }

    const SourcePos source =
        source_from_fields(static_cast<std::uintptr_t>(a1), kOff_enemyPosX, kOff_enemyPosY);
    const SourcePos* source_ptr = source.valid ? &source : nullptr;
    const AimTarget target = select_nearest_eligible_aim_target(source_ptr);
    record_choice(HookSlot::kBounceHelper, target, source_ptr);

    begin_p1_spoof(target, false);
    int result = 0;
    __try {
        result = call_orig_bounce_helper(a1, a2);
    } __finally {
        restore_p1_spoof();
    }
    return result;
}

extern "C" int __stdcall hooked_4224A0_bridge(int a1, int a2, double a3)
{
    if (!g_orig_wrap_helper) return 0;
    if (g_aim_spoof.active) {
        return call_orig_wrap_helper(a1, a2, a3);
    }

    const SourcePos source =
        source_from_fields(static_cast<std::uintptr_t>(a2), kOff_enemyPosX, kOff_enemyPosY);
    const SourcePos* source_ptr = source.valid ? &source : nullptr;
    const AimTarget target = select_nearest_eligible_aim_target(source_ptr);
    record_choice(HookSlot::kWrapHelper, target, source_ptr);

    begin_p1_spoof(target, false);
    int result = 0;
    __try {
        result = call_orig_wrap_helper(a1, a2, a3);
    } __finally {
        restore_p1_spoof();
    }
    return result;
}

extern "C" __declspec(naked) int hooked_4224A0()
{
    __asm {
        sub esp, 8
        fstp qword ptr [esp]
        push ecx
        push edx
        call hooked_4224A0_bridge
        add esp, 16
        ret
    }
}

void __fastcall hooked_422720(int a1, int a2)
{
    if (!g_orig_spawn_gate) return;
    if (g_aim_spoof.active) {
        call_orig_spawn_gate(a1, a2);
        return;
    }

    const SourcePos source =
        source_from_fields(static_cast<std::uintptr_t>(a1), kOff_enemyCalcPosX,
                           kOff_enemyCalcPosY);
    const SourcePos* source_ptr = source.valid ? &source : nullptr;
    const AimTarget target = select_nearest_eligible_aim_target(source_ptr);
    record_choice(HookSlot::kSpawnGate, target, source_ptr);

    begin_p1_spoof(target, false);
    __try {
        call_orig_spawn_gate(a1, a2);
    } __finally {
        restore_p1_spoof();
    }
}

int __fastcall hooked_428310(float* a1, float* a2)
{
    if (!g_orig_vector_helper) return 0;
    if (g_aim_spoof.active) {
        return call_orig_vector_helper(a1, a2);
    }

    SourcePos source;
    if (a1) {
        source.valid = true;
        source.x = a1[145];
        source.y = a1[146];
    }
    const SourcePos* source_ptr = source.valid ? &source : nullptr;
    const AimTarget target = select_nearest_eligible_aim_target(source_ptr);
    record_choice(HookSlot::kVectorHelper, target, source_ptr);

    begin_p1_spoof(target, false);
    int result = 0;
    __try {
        result = call_orig_vector_helper(a1, a2);
    } __finally {
        restore_p1_spoof();
    }
    return result;
}

extern "C" int __stdcall hooked_42C660_bridge(int a1, double a2)
{
    if (!g_orig_enemy_manager_on_update) return 0;
    if (g_aim_spoof.active) {
        return call_orig_enemy_manager_on_update(a1, a2);
    }

    const AimTarget target = select_nearest_eligible_aim_target();
    record_choice(HookSlot::kEnemyManager, target, nullptr);

    begin_p1_spoof(target, false);
    int result = 0;
    __try {
        result = call_orig_enemy_manager_on_update(a1, a2);
    } __finally {
        restore_p1_spoof();
    }
    return result;
}

extern "C" __declspec(naked) int hooked_42C660()
{
    __asm {
        sub esp, 8
        fstp qword ptr [esp]
        push ecx
        call hooked_42C660_bridge
        add esp, 12
        ret
    }
}

extern "C" int __stdcall hooked_416B90_bridge(int a1, double a2)
{
    if (!g_orig_spellcard_on_update) return 0;
    if (g_aim_spoof.active) {
        return call_orig_spellcard_on_update(a1, a2);
    }

    const AimTarget target = select_nearest_eligible_aim_target();
    record_choice(HookSlot::kSpellcard, target, nullptr);

    begin_p1_spoof(target, false);
    int result = 0;
    __try {
        result = call_orig_spellcard_on_update(a1, a2);
    } __finally {
        restore_p1_spoof();
    }
    return result;
}

extern "C" __declspec(naked) int hooked_416B90()
{
    __asm {
        sub esp, 8
        fstp qword ptr [esp]
        push ecx
        call hooked_416B90_bridge
        add esp, 12
        ret
    }
}

int __fastcall hooked_426C40(void* this_, void* /*edx_unused*/)
{
    if (!g_orig_target_acquire_block) return 0;
    if (g_aim_spoof.active) {
        return call_orig_target_acquire_block(this_);
    }

    const AimTarget target = select_nearest_eligible_aim_target();
    record_choice(HookSlot::kTableBlock, target, nullptr);

    begin_p1_spoof(target, false);
    int result = 0;
    __try {
        result = call_orig_target_acquire_block(this_);
    } __finally {
        restore_p1_spoof();
    }
    return result;
}

void cleanup_hooks() noexcept
{
    void* const float_target = resolve(kFloatResolverRVA);
    void* const ptr_target = resolve(kPointerResolverRVA);
    void* const secondary_target = resolve(kSecondaryResolverRVA);
    void* const bounce_target = resolve(kBounceHelperRVA);
    void* const wrap_target = resolve(kWrapHelperRVA);
    void* const spawn_target = resolve(kSpawnGateRVA);
    void* const vector_target = resolve(kVectorHelperRVA);
    void* const manager_target = resolve(kEnemyManagerOnUpdateRVA);
    void* const spell_target = resolve(kSpellcardOnUpdateRVA);
    void* const table_target = resolve(kTargetAcquireBlockRVA);

    if (g_orig_float_resolver && float_target) {
        MH_DisableHook(float_target);
        MH_RemoveHook(float_target);
    }
    if (g_orig_pointer_resolver && ptr_target) {
        MH_DisableHook(ptr_target);
        MH_RemoveHook(ptr_target);
    }
    if (g_orig_secondary_resolver && secondary_target) {
        MH_DisableHook(secondary_target);
        MH_RemoveHook(secondary_target);
    }
    if (g_orig_bounce_helper && bounce_target) {
        MH_DisableHook(bounce_target);
        MH_RemoveHook(bounce_target);
    }
    if (g_orig_wrap_helper && wrap_target) {
        MH_DisableHook(wrap_target);
        MH_RemoveHook(wrap_target);
    }
    if (g_orig_spawn_gate && spawn_target) {
        MH_DisableHook(spawn_target);
        MH_RemoveHook(spawn_target);
    }
    if (g_orig_vector_helper && vector_target) {
        MH_DisableHook(vector_target);
        MH_RemoveHook(vector_target);
    }
    if (g_orig_enemy_manager_on_update && manager_target) {
        MH_DisableHook(manager_target);
        MH_RemoveHook(manager_target);
    }
    if (g_orig_spellcard_on_update && spell_target) {
        MH_DisableHook(spell_target);
        MH_RemoveHook(spell_target);
    }
    if (g_orig_target_acquire_block && table_target) {
        MH_DisableHook(table_target);
        MH_RemoveHook(table_target);
    }

    g_orig_float_resolver = nullptr;
    g_orig_pointer_resolver = nullptr;
    g_orig_secondary_resolver = nullptr;
    g_orig_bounce_helper = nullptr;
    g_orig_wrap_helper = nullptr;
    g_orig_spawn_gate = nullptr;
    g_orig_vector_helper = nullptr;
    g_orig_enemy_manager_on_update = nullptr;
    g_orig_spellcard_on_update = nullptr;
    g_orig_target_acquire_block = nullptr;
}

MH_STATUS create_hook_checked(void* target, void* detour, LPVOID* original,
                              const char* name) noexcept
{
    const MH_STATUS status = MH_CreateHook(target, detour, original);
    if (status != MH_OK) {
        th08_platform::log_line("enemy_aim: MH_CreateHook %s failed status=%d",
                                name, status);
    }
    return status;
}

MH_STATUS enable_hook_checked(void* target, const char* name) noexcept
{
    const MH_STATUS status = MH_EnableHook(target);
    if (status != MH_OK) {
        th08_platform::log_line("enemy_aim: MH_EnableHook %s failed status=%d",
                                name, status);
    }
    return status;
}

}  // namespace

bool install()
{
    if (g_orig_float_resolver || g_orig_pointer_resolver ||
        g_orig_secondary_resolver || g_orig_bounce_helper ||
        g_orig_wrap_helper || g_orig_spawn_gate || g_orig_vector_helper ||
        g_orig_enemy_manager_on_update || g_orig_spellcard_on_update ||
        g_orig_target_acquire_block) {
        th08_platform::log_line("enemy_aim: already installed");
        return true;
    }

    const void* const float_target = resolve(kFloatResolverRVA);
    const void* const ptr_target = resolve(kPointerResolverRVA);
    const void* const secondary_target = resolve(kSecondaryResolverRVA);
    const void* const bounce_target = resolve(kBounceHelperRVA);
    const void* const wrap_target = resolve(kWrapHelperRVA);
    const void* const spawn_target = resolve(kSpawnGateRVA);
    const void* const vector_target = resolve(kVectorHelperRVA);
    const void* const manager_target = resolve(kEnemyManagerOnUpdateRVA);
    const void* const spell_target = resolve(kSpellcardOnUpdateRVA);
    const void* const table_target = resolve(kTargetAcquireBlockRVA);

    if (!float_target || !ptr_target || !secondary_target || !bounce_target ||
        !wrap_target || !spawn_target || !vector_target || !manager_target ||
        !spell_target || !table_target) {
        th08_platform::log_line(
            "enemy_aim: resolve failed (%p %p %p %p %p %p %p %p %p %p)",
            float_target, ptr_target, secondary_target, bounce_target,
            wrap_target, spawn_target, vector_target, manager_target,
            spell_target, table_target);
        return false;
    }

    const unsigned log_budget =
        env_u32("TH08_PLATFORM_ENEMY_AIM_LOG", 0);
    for (auto& hook_counter : g_hook_counters) {
        hook_counter.log_remaining.store(static_cast<long>(log_budget),
                                         std::memory_order_relaxed);
    }

    // Per-hook env gates. Crash diagnosis: when all 10 are enabled the user
    // reported a stage-entry crash. Whole-function spoofs (ENM/SPC) and the
    // table-block hook (TBL) are the most ABI-risky, so they are default-OFF
    // until verified. Function-level helpers (FVAR/PVAR/SEC/BNC/WRP/GATE/VEC)
    // are default-ON because their hot paths went through user testing in
    // earlier batches' siblings (item_routing/dual_collision share the spoof
    // pattern for FVAR/SEC/BNC/etc).
    //
    // Override individually via TH08_PLATFORM_AIM_<NAME>=0 / =1.
    struct AimGate {
        const char* env;
        bool default_on;
        bool wanted;
    };
    AimGate gate_fvar{"TH08_PLATFORM_AIM_FVAR", true,  false};
    AimGate gate_pvar{"TH08_PLATFORM_AIM_PVAR", true,  false};
    AimGate gate_sec {"TH08_PLATFORM_AIM_SEC",  true,  false};
    AimGate gate_bnc {"TH08_PLATFORM_AIM_BNC",  true,  false};
    AimGate gate_wrp {"TH08_PLATFORM_AIM_WRP",  true,  false};
    AimGate gate_gate{"TH08_PLATFORM_AIM_GATE", true,  false};
    AimGate gate_vec {"TH08_PLATFORM_AIM_VEC",  true,  false};
    AimGate gate_enm {"TH08_PLATFORM_AIM_ENM",  false, false};
    AimGate gate_spc {"TH08_PLATFORM_AIM_SPC",  false, false};
    AimGate gate_tbl {"TH08_PLATFORM_AIM_TBL",  false, false};
    auto resolve_gate = [](AimGate& g) {
        g.wanted = env_flag(g.env, g.default_on);
    };
    resolve_gate(gate_fvar); resolve_gate(gate_pvar); resolve_gate(gate_sec);
    resolve_gate(gate_bnc);  resolve_gate(gate_wrp);  resolve_gate(gate_gate);
    resolve_gate(gate_vec);  resolve_gate(gate_enm);  resolve_gate(gate_spc);
    resolve_gate(gate_tbl);

    th08_platform::log_line(
        "enemy_aim: gates FVAR=%d PVAR=%d SEC=%d BNC=%d WRP=%d GATE=%d VEC=%d ENM=%d SPC=%d TBL=%d "
        "(set TH08_PLATFORM_AIM_<NAME>=1/0 to override)",
        gate_fvar.wanted, gate_pvar.wanted, gate_sec.wanted, gate_bnc.wanted,
        gate_wrp.wanted, gate_gate.wanted, gate_vec.wanted, gate_enm.wanted,
        gate_spc.wanted, gate_tbl.wanted);

    th08_platform::log_line(
        "enemy_aim: hooking FVAR=%p PVAR=%p SEC=%p BNC=%p WRP=%p GATE=%p VEC=%p ENM=%p SPC=%p TBL=%p",
        float_target, ptr_target, secondary_target, bounce_target,
        wrap_target, spawn_target, vector_target, manager_target,
        spell_target, table_target);

    if (gate_fvar.wanted &&
        create_hook_checked(const_cast<void*>(float_target),
                            reinterpret_cast<void*>(&hooked_420120),
                            reinterpret_cast<LPVOID*>(&g_orig_float_resolver),
                            "sub_420120") != MH_OK) {
        cleanup_hooks();
        return false;
    }
    if (gate_pvar.wanted &&
        create_hook_checked(const_cast<void*>(ptr_target),
                            reinterpret_cast<void*>(&hooked_420950),
                            reinterpret_cast<LPVOID*>(&g_orig_pointer_resolver),
                            "sub_420950") != MH_OK) {
        cleanup_hooks();
        return false;
    }
    if (gate_sec.wanted &&
        create_hook_checked(const_cast<void*>(secondary_target),
                            reinterpret_cast<void*>(&hooked_41F420),
                            reinterpret_cast<LPVOID*>(&g_orig_secondary_resolver),
                            "sub_41F420") != MH_OK) {
        cleanup_hooks();
        return false;
    }
    if (gate_bnc.wanted &&
        create_hook_checked(const_cast<void*>(bounce_target),
                            reinterpret_cast<void*>(&hooked_422020),
                            reinterpret_cast<LPVOID*>(&g_orig_bounce_helper),
                            "sub_422020") != MH_OK) {
        cleanup_hooks();
        return false;
    }
    if (gate_wrp.wanted &&
        create_hook_checked(const_cast<void*>(wrap_target),
                            reinterpret_cast<void*>(&hooked_4224A0),
                            reinterpret_cast<LPVOID*>(&g_orig_wrap_helper),
                            "sub_4224A0") != MH_OK) {
        cleanup_hooks();
        return false;
    }
    if (gate_gate.wanted &&
        create_hook_checked(const_cast<void*>(spawn_target),
                            reinterpret_cast<void*>(&hooked_422720),
                            reinterpret_cast<LPVOID*>(&g_orig_spawn_gate),
                            "sub_422720") != MH_OK) {
        cleanup_hooks();
        return false;
    }
    if (gate_vec.wanted &&
        create_hook_checked(const_cast<void*>(vector_target),
                            reinterpret_cast<void*>(&hooked_428310),
                            reinterpret_cast<LPVOID*>(&g_orig_vector_helper),
                            "sub_428310") != MH_OK) {
        cleanup_hooks();
        return false;
    }
    if (gate_enm.wanted &&
        create_hook_checked(const_cast<void*>(manager_target),
                            reinterpret_cast<void*>(&hooked_42C660),
                            reinterpret_cast<LPVOID*>(&g_orig_enemy_manager_on_update),
                            "EnemyManager::OnUpdate") != MH_OK) {
        cleanup_hooks();
        return false;
    }
    if (gate_spc.wanted &&
        create_hook_checked(const_cast<void*>(spell_target),
                            reinterpret_cast<void*>(&hooked_416B90),
                            reinterpret_cast<LPVOID*>(&g_orig_spellcard_on_update),
                            "Spellcard::SpellcardOnUpdateImpl") != MH_OK) {
        cleanup_hooks();
        return false;
    }
    if (gate_tbl.wanted &&
        create_hook_checked(const_cast<void*>(table_target),
                            reinterpret_cast<void*>(&hooked_426C40),
                            reinterpret_cast<LPVOID*>(&g_orig_target_acquire_block),
                            "0x426C40 block") != MH_OK) {
        cleanup_hooks();
        return false;
    }

    // Enable phase. cleanup_hooks() inspects g_orig_* pointers, so a hook
    // that never created (gate off) will be safely skipped here too.
    bool enable_ok = true;
    if (g_orig_float_resolver &&
        enable_hook_checked(const_cast<void*>(float_target), "sub_420120") != MH_OK) {
        enable_ok = false;
    }
    if (enable_ok && g_orig_pointer_resolver &&
        enable_hook_checked(const_cast<void*>(ptr_target), "sub_420950") != MH_OK) {
        enable_ok = false;
    }
    if (enable_ok && g_orig_secondary_resolver &&
        enable_hook_checked(const_cast<void*>(secondary_target), "sub_41F420") != MH_OK) {
        enable_ok = false;
    }
    if (enable_ok && g_orig_bounce_helper &&
        enable_hook_checked(const_cast<void*>(bounce_target), "sub_422020") != MH_OK) {
        enable_ok = false;
    }
    if (enable_ok && g_orig_wrap_helper &&
        enable_hook_checked(const_cast<void*>(wrap_target), "sub_4224A0") != MH_OK) {
        enable_ok = false;
    }
    if (enable_ok && g_orig_spawn_gate &&
        enable_hook_checked(const_cast<void*>(spawn_target), "sub_422720") != MH_OK) {
        enable_ok = false;
    }
    if (enable_ok && g_orig_vector_helper &&
        enable_hook_checked(const_cast<void*>(vector_target), "sub_428310") != MH_OK) {
        enable_ok = false;
    }
    if (enable_ok && g_orig_enemy_manager_on_update &&
        enable_hook_checked(const_cast<void*>(manager_target), "EnemyManager::OnUpdate") != MH_OK) {
        enable_ok = false;
    }
    if (enable_ok && g_orig_spellcard_on_update &&
        enable_hook_checked(const_cast<void*>(spell_target), "Spellcard::SpellcardOnUpdateImpl") != MH_OK) {
        enable_ok = false;
    }
    if (enable_ok && g_orig_target_acquire_block &&
        enable_hook_checked(const_cast<void*>(table_target), "0x426C40 block") != MH_OK) {
        enable_ok = false;
    }
    if (!enable_ok) {
        cleanup_hooks();
        return false;
    }

    th08_platform::log_line(
        "enemy_aim: enabled (alternate-by-frame fallback for source-less resolver/manager/spell hooks)");
    return true;
}

void uninstall()
{
    cleanup_hooks();
}

Counters snapshot_counters()
{
    Counters counters;
    counters.calls_total = g_aim_calls_total.load(std::memory_order_relaxed);

    auto copy_pair = [&](PairCounters& out, HookSlot slot) {
        const auto index = static_cast<std::size_t>(slot);
        out.p1 = g_hook_counters[index].p1.load(std::memory_order_relaxed);
        out.p2 = g_hook_counters[index].p2.load(std::memory_order_relaxed);
    };

    copy_pair(counters.float_resolver, HookSlot::kFloatResolver);
    copy_pair(counters.pointer_resolver, HookSlot::kPointerResolver);
    copy_pair(counters.secondary_resolver, HookSlot::kSecondaryResolver);
    copy_pair(counters.bounce_helper, HookSlot::kBounceHelper);
    copy_pair(counters.wrap_helper, HookSlot::kWrapHelper);
    copy_pair(counters.spawn_gate, HookSlot::kSpawnGate);
    copy_pair(counters.vector_helper, HookSlot::kVectorHelper);
    copy_pair(counters.enemy_manager, HookSlot::kEnemyManager);
    copy_pair(counters.spellcard, HookSlot::kSpellcard);
    copy_pair(counters.table_block, HookSlot::kTableBlock);
    return counters;
}

}  // namespace th08_platform::state::enemy_aim
