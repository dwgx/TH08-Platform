#pragma once

#include <cstddef>
#include <cstdint>

namespace th08_platform::globals {

inline constexpr std::uintptr_t kAddr_g_GameManager = 0x0160f508;
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
inline constexpr std::size_t kSize_ZunGlobals = 0x00e4;
inline constexpr std::size_t kSize_Rng = 0;  // TBD: no sizeof C_ASSERT found in decomp.
inline constexpr std::size_t kSize_CurFrameInput = 0x0002;
inline constexpr std::size_t kSize_LastFrameInput = 0x0002;
inline constexpr std::size_t kSize_IsEighthFrameOfHeldInput = 0x0002;
inline constexpr std::size_t kSize_NumOfFramesInputsWereHeld = 0x0002;
inline constexpr std::size_t kSize_Chain = 0;  // TBD: no sizeof C_ASSERT found in decomp.
inline constexpr std::size_t kSize_ControllerData = 0x0080;  // Global.cpp exposes a 32*4-byte array.
inline constexpr std::size_t kSize_ItemManager = 0x17b094;
inline constexpr std::size_t kSize_EnemyManager = 0;  // TBD: no sizeof C_ASSERT found in decomp.
inline constexpr std::size_t kSize_BulletManager = 0;  // TBD: no sizeof C_ASSERT found in decomp.
inline constexpr std::size_t kSize_Background = 0x6600;
inline constexpr std::size_t kSize_AsciiManager = 0x171b0;
inline constexpr std::size_t kSize_Supervisor = 0x0364;

template <typename T = void>
[[nodiscard]] T* at(std::uintptr_t addr) noexcept
{
    return reinterpret_cast<T*>(addr);
}

}  // namespace th08_platform::globals
