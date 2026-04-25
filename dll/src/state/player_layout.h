#pragma once

// Player struct layout for TH08 IN 1.00d.
// Source of truth: IDA decompilation of 0x449ca0 (ctor), 0x44c230
// (RegisterChain), 0x44c390 (real OnUpdate), 0x44d650 (AddedCallback),
// 0x44a5a0 (CalcItemBoxCollision), 0x44dc60 (DeletedCallback).
//
// Verified facts:
//   sizeof(Player)  == 0xE2B30 (928560 bytes) — from RegisterChain memset.
//   &g_Player       == 0x17D5EF8.
//   Player is a singleton; ZUN's binary also has g_Player1ShtFiles +
//   g_Player2ShtFile data tables (vestigial 2P slot — see CalcItemBoxCollision
//   off_4C7AD0 + AddedCallback's per-character sht load), but no g_Player2
//   instance exists in the binary. Phase 5 must allocate one.
//
// This header documents discovered field offsets. It does NOT define
// `struct Player` — the canonical layout lives in the decomp at
// `th08-decomp/src/Player.hpp` and we only have ~5% of fields named there.
// Use these byte-offset constants when writing DLL code that touches
// Player; do not introduce a parallel struct that could drift.

#include <cstddef>
#include <cstdint>

namespace th08_platform::player_layout {

// Byte offsets within the Player struct (singleton at kAddr_g_Player).
inline constexpr std::size_t kOff_playerState                = 0x0000;  // i8 (0=alive, 1=spawning, 2=dead, plus 3/4 used in CalcItemBoxCollision)
inline constexpr std::size_t kOff_unkByteAt0x02              = 0x0002;  // i8 — set to 1 in AddedCallback after sht load
inline constexpr std::size_t kOff_pBaseAnmVm                 = 0x000C;  // *(this+3) in AddedCallback — primary AnmVm pointer (sub_465A40 result)
inline constexpr std::size_t kOff_anmVmInline                = 0x0010;  // AnmVm passed to ExecuteScript in OnUpdate; also subject to sub_4069F0(this+4) (likely AnmVm body, ~4 dwords)

// Per-character sht buffers (qmemcpy from off_4C7AD0 stride-10 table).
inline constexpr std::size_t kOff_shtBuf1                    = 0x0400;  // 20 bytes from &off_4C7AD0[10*characterIdx]
inline constexpr std::size_t kOff_shtBuf2                    = 0x0405;  // overlap by design (5-byte stride, ZUN packing)

// Live world position (verified by codex 5e batch via Player::FUN_0044d420
// boundary clamp at 0x44D420 and Player::Player ctor's `unknown_libname_1
// (this+692)` D3DXVECTOR3 init):
inline constexpr std::size_t kOff_pos_x                      = 0x02B4;  // float - read by FUN_0044AEC0 movement code
inline constexpr std::size_t kOff_pos_y                      = 0x02B8;  // float
inline constexpr std::size_t kOff_pos_z                      = 0x02BC;  // float (likely; D3DXVECTOR3)

// Per-frame movement direction stored at +0xE2A98 (=+232090 dword)
// (verified codex 5e puzzle 2: Player::FUN_0044AEC0 line ~0x44AF82
// decodes g_CurFrameInput bits and writes 0/1/2/3/4 here):
inline constexpr std::size_t kOff_moveDirection              = 0xE2A98;  // i32 (0=none, 1=up, 2=down, 3=left, 4=right)

// Position / hitbox AABB (floats indexed in CalcItemBoxCollision via float* this).
// Pattern: this[239]<=max.x && this[242]>=min.x && this[240]<=max.y && this[243]>=min.y
inline constexpr std::size_t kOff_hitboxMinX                 = 0x03BC;  // float (player[239])
inline constexpr std::size_t kOff_hitboxMinY                 = 0x03C0;  // float (player[240])
inline constexpr std::size_t kOff_hitboxMinZ_or_pad          = 0x03C4;  // float (player[241]) — read but not used in 2D box test
inline constexpr std::size_t kOff_hitboxMaxX                 = 0x03C8;  // float (player[242])
inline constexpr std::size_t kOff_hitboxMaxY                 = 0x03CC;  // float (player[243])
inline constexpr std::size_t kOff_hitboxMaxZ_or_pad          = 0x03D0;  // float (player[244])

// IMPORTANT: the +245..+253 fields below are NOT current position
// (despite my earlier guess). Runtime test showed they stay constant
// at (0.82, 1.40) across 2400+ frames while the actual player moved.
// The live pos is at +0x2B4/+0x2B8 (see kOff_pos_x above).
// These +245..+253 fields are likely some "spawn anchor" or normalized
// scaling values from the sht file - keep the table for reference only.
inline constexpr std::size_t kOff_posOrVel_245               = 0x03D4;  // = *(this+246)
inline constexpr std::size_t kOff_posOrVel_246               = 0x03D8;  // = flt_164D2E4 / 2.0  (probably PLAYABLE_AREA width / 2)
inline constexpr std::size_t kOff_posOrVel_247               = 0x03DC;  // = 1084227584 (5.0f as int)
inline constexpr std::size_t kOff_posOrVel_248               = 0x03E0;
inline constexpr std::size_t kOff_posOrVel_249               = 0x03E4;  // = *(dword_18B896C+4)/2.0  (ShtFile-derived spawn coord)
inline constexpr std::size_t kOff_posOrVel_250               = 0x03E8;  // = 5.0f
inline constexpr std::size_t kOff_posOrVel_251               = 0x03EC;
inline constexpr std::size_t kOff_posOrVel_252               = 0x03F0;  // = *(dword_18B896C+6)/2.0
inline constexpr std::size_t kOff_posOrVel_253               = 0x03F4;  // = 5.0f

// Power / focus / "speed mode":
inline constexpr std::size_t kOff_speedScaleA                = 0x0404;  // = 1065353216 (1.0f)
inline constexpr std::size_t kOff_speedScaleB                = 0x0408;  // = 1065353216 (1.0f)

// Bullet array (FixedArray of 756 elements * 4 bytes):
inline constexpr std::size_t kOff_bulletArrayA               = 0x040C;  // sub_406850(., 756, 4, sub_449E50)
inline constexpr std::size_t kOff_objAt4060                  = 0x0FDC;  // sub_449EA0(this + 4060) — sub-object init

// Primary weapon AnmVm pointer (set to NULL or AnmVm* in OnUpdate logic):
inline constexpr std::size_t kOff_primaryWeaponAnmVm         = 0xBE834;  // *(this+195085)

// Bigger bullet/laser array (1156 elements * 128 bytes = 147968 bytes):
inline constexpr std::size_t kOff_bigBulletArray             = 0xBE838;  // sub_406850(., 1156, 128, sub_449EF0)
inline constexpr std::size_t kSize_bigBulletElement          = 128;
inline constexpr std::size_t kCount_bigBulletElements        = 1156;

// Tail section (initialized after the big bullet array):
inline constexpr std::size_t kOff_subArrayA                  = 0xE2A78;  // sub_406850(., 16, 3, sub_449F50) — 16 of 3-byte? Or table.
                                                                          // Actually +928312 = 0xE2A78 in ctor; AddedCallback uses +232090 = 0xE2A98
inline constexpr std::size_t kOff_unkAt232090                = 0xE2A98;  // = *((DWORD*)dword_18B896C + 2)
inline constexpr std::size_t kOff_shtFile1Slot               = 0xE2A74;  // (DWORD) — heap pointer freed by DeletedCallback
inline constexpr std::size_t kOff_shtFile2Slot               = 0xE2A78;  // (DWORD) — heap pointer freed by DeletedCallback
inline constexpr std::size_t kOff_unkAt232102                = 0xE2AC0;  // set to 0 in AddedCallback
inline constexpr std::size_t kOff_unkAt232131                = 0xE2B0C;  // set to -1077342245 (= -0.5f as int) in AddedCallback
inline constexpr std::size_t kOff_secondaryWeaponAnmVm       = 0xE2B24;  // *(this+232137)

// (More fields exist in the gap 0xE2B28..0xE2B30 — only 8 bytes left, likely
// padding or one final pointer/flag.)

// Lifecycle: see Player::AddedCallback (0x44d650) for full init sequence,
// and Player::DeletedCallback (0x44dc60) for tear-down. The two heap pointers
// (shtFile1Slot, shtFile2Slot) are the only non-trivial heap state in
// Player; everything else is in-place inside the 0xE2B30 byte block.

}  // namespace th08_platform::player_layout
