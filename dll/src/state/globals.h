#pragma once

#include <cstddef>
#include <cstdint>

namespace th08_platform::globals {

inline constexpr std::uintptr_t kAddr_g_GameManager = 0x0160f508;
inline constexpr std::uintptr_t kAddr_g_Player = 0x017d5ef8;
inline constexpr std::uintptr_t kAddr_g_Rng = 0x0164d520;
inline constexpr std::uintptr_t kAddr_g_CurFrameInput = 0x0164d528;
inline constexpr std::uintptr_t kAddr_g_LastFrameInput = 0x0164d530;
inline constexpr std::uintptr_t kAddr_g_IsEighthFrameOfHeldInput = 0x0164d538;
inline constexpr std::uintptr_t kAddr_g_NumOfFramesInputsWereHeld = 0x0164d53c;
inline constexpr std::uintptr_t kAddr_g_Chain = 0x0164f548;
inline constexpr std::uintptr_t kAddr_g_ControllerData = 0x016535a0;
inline constexpr std::uintptr_t kAddr_g_ItemManager = 0x01653648;
inline constexpr std::uintptr_t kAddr_g_EnemyManager = 0x00577f20;
inline constexpr std::uintptr_t kAddr_g_BulletManager = 0x00f54e90;
inline constexpr std::uintptr_t kAddr_g_Background = 0x004e4030;
inline constexpr std::uintptr_t kAddr_g_AsciiManager = 0x004cce20;
inline constexpr std::uintptr_t kAddr_g_Supervisor = 0x017ce758;

inline constexpr std::size_t kOffset_GameManager_globals = 0x0008;

inline constexpr std::size_t kSize_GameManager = 0x3de3c;
// Verified by IDA: Player::RegisterChain @ 0x44c230 calls
//   memset(&byte_17D5EF8, 0, 0xE2B30u)
// to clear g_Player on init. So sizeof(Player) is EXACTLY 0xE2B30 bytes.
// This includes two heap-pointer FIELDS at +0xE2A74 / +0xE2A78 (shtFile1/2
// slots, freed by Player::DeletedCallback) — those buffers themselves
// must be heap-journaled for rollback (Phase 4 already supports this).
inline constexpr std::size_t kSize_Player = 0xE2B30;
inline constexpr std::size_t kOffset_Player_shtFile1Slot = 0xE2A74;
inline constexpr std::size_t kOffset_Player_shtFile2Slot = 0xE2A78;
inline constexpr std::size_t kSize_ZunGlobals = 0x00e4;
inline constexpr std::size_t kSize_Rng = 0x0008;  // Verified: u16 seed + u32 generationCount + 2B pad.
inline constexpr std::size_t kSize_CurFrameInput = 0x0002;
inline constexpr std::size_t kSize_LastFrameInput = 0x0002;
inline constexpr std::size_t kSize_IsEighthFrameOfHeldInput = 0x0002;
inline constexpr std::size_t kSize_NumOfFramesInputsWereHeld = 0x0002;
inline constexpr std::size_t kSize_Chain = 0x0040;  // Verified: 2 * sizeof(ChainElem) where ChainElem = 32B.
inline constexpr std::size_t kSize_ControllerData = 0x0080;  // Global.cpp exposes a 32*4-byte array.
inline constexpr std::size_t kSize_ItemManager = 0x17b094;
// EnemyManager + BulletManager: decomp's hpp declares only methods, no
// fields, so no C_ASSERT exists. Sizes inferred from address-subtraction
// to the next named global in reccmp-symbols.csv:
//   g_EnemyManager  (0x00577f20) -> g_EnemyManagerCalcChain (0x00f54e30) = 0x9DCF10
//   g_BulletManager (0x00f54e90) -> g_BulletManagerCalcChain(0x0160f408) = 0x6BA578
// Layout-safe: no other named globals fall between, so capturing this many
// bytes does NOT overrun into adjacent state. Heavy (~17 MB combined, mostly
// inactive enemy/bullet slots) — Phase 4 may optimize via delta/sparse capture.
inline constexpr std::size_t kSize_EnemyManager = 0x9DCF10;
inline constexpr std::size_t kSize_BulletManager = 0x6BA578;
inline constexpr std::size_t kSize_Background = 0x6600;
inline constexpr std::size_t kSize_AsciiManager = 0x171b0;
inline constexpr std::size_t kSize_Supervisor = 0x0364;

template <typename T = void>
[[nodiscard]] T* at(std::uintptr_t addr) noexcept
{
    return reinterpret_cast<T*>(addr);
}

}  // namespace th08_platform::globals
