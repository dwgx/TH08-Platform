#pragma once

// Phase 6b wire format. Modeled on RUEEE/th06_multi_net's Connection.hpp
// (https://github.com/RUEEE/th06_multi_net) so the on-wire bytes are
// theoretically interchangeable with TH06 peers running RUEEE's decomp
// fork. Layout, packing and field order MUST stay identical to RUEEE's
// upstream — the audit at docs/6b_lockstep_audit.md spells out why.

#include <cstdint>
#include <cstring>

namespace th08_platform::net::protocol {

inline constexpr int kKeyPackFrameNum = 15;
inline constexpr std::uint32_t kMultiNetVer = 3950;  // matches RUEEE 3.9.5

#pragma pack(push, 1)

template<int N_Bits>
struct Bits {
    std::uint8_t data[N_Bits / 8];

    int Read(int n_bit) const
    {
        return (data[n_bit >> 3] >> (n_bit & 7)) & 1u;
    }
    void Write(int n_bit, int bit)
    {
        std::uint8_t& c = data[n_bit >> 3];
        const std::uint8_t mask = static_cast<std::uint8_t>(1u << (n_bit & 7));
        c = static_cast<std::uint8_t>((c & ~mask) | ((-(bit & 1)) & mask));
    }
    void Clear()
    {
        std::memset(data, 0, sizeof(data));
    }
};

inline void ReadFromInt(Bits<16>& b, std::uint16_t i)
{
    b.data[0] = static_cast<std::uint8_t>(i & 0xFF);
    b.data[1] = static_cast<std::uint8_t>((i >> 8) & 0xFF);
}

inline void WriteToInt(const Bits<16>& b, std::uint16_t& i)
{
    i = static_cast<std::uint16_t>(b.data[0]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(b.data[1]) << 8);
}

enum Control : std::int32_t {
    Ctrl_No_Ctrl         = 0,
    Ctrl_Start_Game      = 1,
    Ctrl_Key             = 2,
    Ctrl_Set_InitSetting = 3,
    Ctrl_Try_Resync      = 4,
    Ctrl_Ghost           = 5,
};

enum InGameCtrlType : std::int32_t {
    Quick_Quit    = 0,
    Quick_Restart = 1,
    Inf_Life      = 2,
    Inf_Bomb      = 3,
    Inf_Power     = 4,
    Add_Delay     = 5,
    Dec_Delay     = 6,
    Insane_Mode   = 7,
    IGC_NONE      = 8,
};

struct CtrlPack {
    std::int32_t frame;
    Control ctrl_type;
    union {
        Bits<16> keys[kKeyPackFrameNum];
        struct { std::int32_t delay; std::int32_t ver; } init_setting;
        struct { std::int32_t frame_to_re_sync; } resync_setting;
        struct {
            float pos_x;             // +0:  Player.pos.x (g_Player+0x2B4)
            float pos_y;             // +4:  Player.pos.y (g_Player+0x2B8)
            std::uint16_t lives;     // +8:  GM->stats[0x74], truncated float
            std::uint16_t bombs;     // +10: GM->stats[0x80], truncated float
            std::uint16_t power;     // +12: GM->stats[0x98], truncated float
            std::uint16_t pad;       // +14: alignment
            std::uint32_t score;     // +16: GM->stats[0x08], score / 10 in ZUN's units
            std::uint16_t rng_state; // +20: 6g.1 *(uint16*)0x0164D520 — sender's RNG state
            std::uint16_t pad2;      // +22: alignment
        } ghost_pos;                 // total 24 bytes; under union 30B max
    };
    InGameCtrlType igc_type[kKeyPackFrameNum];
    std::uint16_t rng_seed[kKeyPackFrameNum];

    CtrlPack() : frame(0), ctrl_type(Ctrl_No_Ctrl)
    {
        std::memset(keys, 0, sizeof(keys));
        for (int i = 0; i < kKeyPackFrameNum; ++i) {
            igc_type[i] = IGC_NONE;
        }
        std::memset(rng_seed, 0, sizeof(rng_seed));
    }
};

enum PackType : std::int32_t {
    PACK_HELLO = 1,
    PACK_PING  = 2,
    PACK_PONG  = 3,
    PACK_USUAL = 4,
};

struct Pack {
    std::int32_t  type;
    std::uint32_t seq;
    std::uint64_t sendTick;
    std::uint64_t echoTick;
    CtrlPack ctrl;

    Pack() : type(0), seq(0), sendTick(0), echoTick(0), ctrl() {}
};

#pragma pack(pop)

// 4 + 4 + 8 + 8 + (4 + 4 + 30 + 60 + 30) = 152 bytes. Static-asserted at
// the lockstep.cpp translation unit so a layout drift fails compile.
inline constexpr std::size_t kExpectedPackSize = 152;

}  // namespace th08_platform::net::protocol
