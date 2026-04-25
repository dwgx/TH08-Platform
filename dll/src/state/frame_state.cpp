#include "frame_state.h"

#include "globals.h"
#include "../logging.h"

#include <array>
#include <cstring>

namespace th08_platform::state {
namespace {

struct RegionSpec {
    std::uintptr_t address;
    std::size_t size;
    const char* label;
};

constexpr std::array kStaticRegionSpecs{
    RegionSpec{globals::kAddr_g_GameManager, globals::kSize_GameManager, "g_GameManager"},
    RegionSpec{globals::kAddr_g_Player, globals::kSize_Player, "g_Player"},
    RegionSpec{globals::kAddr_g_Rng, globals::kSize_Rng, "g_Rng"},
    RegionSpec{globals::kAddr_g_CurFrameInput, globals::kSize_CurFrameInput, "g_CurFrameInput"},
    RegionSpec{globals::kAddr_g_LastFrameInput, globals::kSize_LastFrameInput, "g_LastFrameInput"},
    RegionSpec{globals::kAddr_g_IsEighthFrameOfHeldInput, globals::kSize_IsEighthFrameOfHeldInput,
               "g_IsEighthFrameOfHeldInput"},
    RegionSpec{globals::kAddr_g_NumOfFramesInputsWereHeld, globals::kSize_NumOfFramesInputsWereHeld,
               "g_NumOfFramesInputsWereHeld"},
    RegionSpec{globals::kAddr_g_Chain, globals::kSize_Chain, "g_Chain"},
    RegionSpec{globals::kAddr_g_ControllerData, globals::kSize_ControllerData, "g_ControllerData"},
    RegionSpec{globals::kAddr_g_ItemManager, globals::kSize_ItemManager, "g_ItemManager"},
    RegionSpec{globals::kAddr_g_EnemyManager, globals::kSize_EnemyManager, "g_EnemyManager"},
    RegionSpec{globals::kAddr_g_BulletManager, globals::kSize_BulletManager, "g_BulletManager"},
    RegionSpec{globals::kAddr_g_Background, globals::kSize_Background, "g_Background"},
    RegionSpec{globals::kAddr_g_AsciiManager, globals::kSize_AsciiManager, "g_AsciiManager"},
    RegionSpec{globals::kAddr_g_Supervisor, globals::kSize_Supervisor, "g_Supervisor"},
};

void save_fpu_state(std::uint8_t* buffer) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        mov eax, buffer
        fnsave [eax]
        fwait
        frstor [eax]
    }
#else
#error Phase 1 FPU capture requires MSVC x86.
#endif
}

void restore_fpu_state(const std::uint8_t* buffer) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        mov eax, buffer
        frstor [eax]
    }
#else
#error Phase 1 FPU restore requires MSVC x86.
#endif
}

void capture_region(FrameState& state, const char* label, std::uintptr_t address, std::size_t size)
{
    CapturedRegion region;
    region.address = address;
    region.size = size;
    region.label = label;

    if (address != 0 && size != 0) {
        region.bytes.resize(size);
        std::memcpy(region.bytes.data(), reinterpret_cast<const void*>(address), size);
    }

    state.regions.push_back(region);
}

void log_tbd_size_warnings_once() noexcept
{
    static bool logged = false;
    if (logged) {
        return;
    }
    logged = true;

    for (const RegionSpec& spec : kStaticRegionSpecs) {
        if (spec.size == 0) {
            th08_platform::log_line("state warning: %s size is TBD; capture skipped", spec.label);
        }
    }
}

void capture_dynamic_zun_globals(FrameState& state)
{
    const auto globals_member_addr = globals::kAddr_g_GameManager + globals::kOffset_GameManager_globals;
    void* const globals_ptr = *globals::at<void*>(globals_member_addr);

    if (globals_ptr == nullptr) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            th08_platform::log_line("state warning: *g_GameManager.globals is null; capture skipped");
        }
        capture_region(state, "*g_GameManager.globals", 0, 0);
        return;
    }

    capture_region(state, "*g_GameManager.globals",
                   reinterpret_cast<std::uintptr_t>(globals_ptr), globals::kSize_ZunGlobals);
}

bool debug_verify_restore_roundtrip() noexcept
{
    std::uint8_t a[8]{0, 1, 2, 3, 4, 5, 6, 7};
    std::uint8_t b[4]{9, 8, 7, 6};

    FrameState test_state;
    save_fpu_state(test_state.fpu.data());
    test_state.regions.reserve(2);
    capture_region(test_state, "test_a", reinterpret_cast<std::uintptr_t>(a), sizeof(a));
    capture_region(test_state, "test_b", reinterpret_cast<std::uintptr_t>(b), sizeof(b));

    std::memset(a, 0xAA, sizeof(a));
    std::memset(b, 0xBB, sizeof(b));

    restore(test_state);

    const std::uint8_t expected_a[8]{0, 1, 2, 3, 4, 5, 6, 7};
    const std::uint8_t expected_b[4]{9, 8, 7, 6};
    return std::memcmp(a, expected_a, sizeof(a)) == 0 &&
           std::memcmp(b, expected_b, sizeof(b)) == 0;
}

void log_restore_self_test_once() noexcept
{
    static bool logged = false;
    if (logged) {
        return;
    }
    logged = true;

    th08_platform::log_line("state restore self-test: %s",
                            debug_verify_restore_roundtrip() ? "ok" : "FAILED");
}

// If these fail to compile, the decomp's struct sizes drifted from what
// the binary actually has - investigate before changing them.
static_assert(globals::kSize_GameManager == 0x3de3c);
static_assert(globals::kSize_Player == 0xE2B30);
static_assert(globals::kSize_ZunGlobals == 0x00e4);
static_assert(globals::kSize_CurFrameInput == 0x0002);
static_assert(globals::kSize_LastFrameInput == 0x0002);
static_assert(globals::kSize_IsEighthFrameOfHeldInput == 0x0002);
static_assert(globals::kSize_NumOfFramesInputsWereHeld == 0x0002);
static_assert(globals::kSize_ControllerData == 0x0080);
static_assert(globals::kSize_ItemManager == 0x17b094);
static_assert(globals::kSize_Background == 0x6600);
static_assert(globals::kSize_AsciiManager == 0x171b0);
static_assert(globals::kSize_Supervisor == 0x0364);

}  // namespace

void capture(FrameState& state, std::uint64_t frame_number)
{
    state.frame_number = frame_number;
    state.regions.clear();
    state.regions.reserve(kStaticRegionSpecs.size() + 1);

    log_tbd_size_warnings_once();
    log_restore_self_test_once();

    save_fpu_state(state.fpu.data());

    capture_region(state, kStaticRegionSpecs[0].label, kStaticRegionSpecs[0].address, kStaticRegionSpecs[0].size);
    capture_dynamic_zun_globals(state);
    for (std::size_t i = 1; i < kStaticRegionSpecs.size(); ++i) {
        const RegionSpec& spec = kStaticRegionSpecs[i];
        capture_region(state, spec.label, spec.address, spec.size);
    }
}

void restore(const FrameState& state)
{
    if (state.regions.empty()) {
        return;
    }

    restore_fpu_state(state.fpu.data());

    for (const CapturedRegion& region : state.regions) {
        if (region.address == 0 || region.size == 0 || region.bytes.size() != region.size) {
            continue;
        }

        std::memcpy(reinterpret_cast<void*>(region.address), region.bytes.data(), region.size);
    }
}

std::size_t total_payload_bytes(const FrameState& state) noexcept
{
    std::size_t total = 0;
    for (const CapturedRegion& region : state.regions) {
        total += region.size;
    }
    return total;
}

}  // namespace th08_platform::state
