# Phase 6b — Architecture audit: RUEEE vs our Phase 4 lockstep

> Per `feedback_architecture_first.md`. Read RUEEE's `Connection.hpp` /
> `Connection.cpp` / `ConnectionUI.cpp` / `Controller.cpp` BEFORE writing
> any 6b code. Output 1-page comparison. Get sign-off. Then dispatch.

## 1. RUEEE protocol summary (TH06 v1.02h fork, last commit 2026-03-22)

### Wire format (`Connection.hpp`)

```cpp
#pragma pack(push, 1)

template<int N_Bits> struct Bits { unsigned char data[N_Bits/8]; ... };

struct CtrlPack {
    int frame;                              // last frame in this batch
    Control ctrl_type;                      // No/Start/Key/InitSetting/Resync
    union {
        Bits<16> keys[KeyPackFrameNum=15];  // 15 frames of inputs
        struct { int delay; int ver; } init_setting;
        struct { int frame_to_re_sync; } resync_setting;
    };
    InGameCtrlType igc_type[15];            // F2/F3/F4/Q/R/M/N per frame
    unsigned short rng_seed[15];            // RNG seed per frame
};

struct Pack {
    int type;          // 1=HELLO 2=PING 3=PONG 4=usual_trans
    unsigned int seq;
    ULONGLONG sendTick;
    ULONGLONG echoTick;
    CtrlPack ctrl;
};
#pragma pack(pop)
```

Sent verbatim via `sendto((char*)&pack, sizeof(Pack))`. ~240 bytes/packet.
@ 4 packets/sec (60fps / 15-frame-stride) = ~960 B/s upstream per peer.

### State machine

- **Lobby (`ConnectionUI.cpp`)**: Win32 dialog, Win32 timer @ 15ms.
  Host listens, Guest connects. Both exchange PING/PONG until connected.
  Click "Start Game" → send `Ctrl_Start_Game` → dialog destroys → game runs.
- **Game-time (`Controller.cpp`)**: per-frame
  - `Controller::SendKeys(frame)`: pack last 15 frames of own keys
    (`g_ctrl_bits_self[frame-i]`) + RNG + igc → send.
  - `Controller::RcvPacks()`: drain UDP, store remote frames into
    `g_ctrl_bits_rcved` / `g_ctrl_rng_rcved` / `g_ctrl_rcved` maps.
- **Delay-based lockstep**: `g_delay` frames; each peer applies own input
  delayed by `g_delay`. Remote input arrives within window.
- **Resync**: `Ctrl_Try_Resync` packet → seek to common safe frame.

### Asymmetry: Host vs Guest

| | Host | Guest |
|---|---|---|
| First action | Bind, wait for guest ping | Bind, send ping to host |
| Learns peer addr | From first guest packet | Set upfront from CLI |
| State | listen-only until first packet | knows host upfront |

## 2. Our Phase 4 baseline (`dll/src/net/lockstep.{cpp,h}`)

### What we have

| Feature | Status |
|---|---|
| UDP socket abstraction (`udp_socket.cpp`) | ✅ |
| `LockstepState` with mutex + sockaddr_in + peer_known + connection enum | ✅ |
| handshake state machine (`wait_for_handshake_locked` / `handle_packet_locked`) | ✅ |
| `peer_known=false` listen-only path (Phase 6a) | ✅ |
| Frame state capture/restore (`frame_state.cpp`) | ✅ unused for 6b |
| Rollback skeleton (`rollback.cpp`) | ✅ unused for 6b |
| Fake RTT injection (`fake_rtt.cpp`) | ✅ |
| CRC32 (`crc32.h`) | ✅ unused for 6b |

### What we're missing

| Gap | Severity | 6b sub-task |
|---|---|---|
| `Bits<N>` template | low (new file) | 6b.1 |
| `Pack` / `CtrlPack` struct | high (rewrite encode/decode) | 6b.1 |
| 15-frame batching | high | 6b.1 |
| 4 pack types (HELLO/PING/PONG/usual) | medium | 6b.1 |
| Background pump thread (16ms) | **CRITICAL** — without this, no packets pump | 6b.2 |
| Per-frame local input capture | high | 6b.3 |
| `Controller::SendKeys(frame)` per 15 frames | high | 6b.4 |
| `Controller::RcvPacks()` drain into maps | high | 6b.5 |
| Per-frame remote input apply | DEFERRED to 6c (needs hooks/input wiring) | — |
| RTT measurement via PING/PONG | medium | 6b.6 |

### Wire format compat

Our Phase 4 used a custom 20-byte `TH8L` encoding (magic + version + flags
+ frame + input + checksum). Phase 6b **replaces** this with RUEEE's `Pack`
serialization. Reasons:

- **Wire-compat with RUEEE TH06 peers** (theoretical — they're TH06, we're
  TH08, but same protocol means future cross-game routing is plausible).
- 15-frame batching cuts UDP rate from 60Hz to 4Hz; one drop = 1 retry.
- Pack types fit RTT measurement (PING/PONG) cleanly.
- CtrlPack already has slots for RNG seed (6c) and resync (6g) without
  more protocol revisions.

Phase 4's `TH8L` was skeleton — no real users — so wholesale replacement
costs nothing.

## 3. 6b scope (this sub-phase only)

### Do

1. Adopt RUEEE wire format: new `dll/src/net/protocol.h` with `Bits<N>`,
   `CtrlPack`, `Pack`, encode/decode.
2. Rewrite `lockstep.cpp` send/recv to use `Pack`.
3. Add background pump thread in `dll_init_thread` calling `net::poll()`
   every ~16ms.
4. Capture local input per frame (hook `Controller::GetInput` already
   exists at `hooks/input.cpp`); store in `g_ctrl_bits_self[frame]`.
5. Every 15 frames, call equivalent of `Controller::SendKeys(frame)`.
6. On recv `Ctrl_Key` packet, store remote frames into
   `g_ctrl_bits_rcved` map.
7. Verify host↔peer handshake completes via PING/PONG; log connected RTT
   (no game start; RTT visible in log).

### Don't (deferred)

| Sub | What | Why deferred |
|---|---|---|
| 6c | Apply remote input to a P2 sprite + shared RNG seed | Needs IDA on g_Rng + 6d's peer ghost first |
| 6d | Peer ghost rendering | Separate concern, distinct LOC |
| 6e | Shared HUD | After 6d ghost works |
| 6f | Match start / lobby UI | Will use existing env-var flow until then |
| 6g | Desync / resync trigger | After 6c gameplay sync proven |

## 4. Implementation files

| Action | File | LOC est |
|---|---|---|
| New | `dll/src/net/protocol.h` (Bits/Pack/CtrlPack + encode/decode) | 150 |
| Rewrite | `dll/src/net/lockstep.cpp` (use Pack) | 300 (replace ~250) |
| Modify | `dll/src/net/lockstep.h` (new exports) | +10 |
| Modify | `dll/src/main.cpp` (spawn pump thread) | +15 |
| New | `dll/src/net/pump_thread.cpp` (16ms tick → net::poll) | 60 |
| Modify | `dll/src/hooks/input.cpp` (capture local input per frame, send every 15) | +30 |
| Modify | `dll/CMakeLists.txt` (add pump_thread.cpp + protocol.h) | +1 |

**Est**: ~600 net LOC, mostly mechanical.

## 5. Risks

| Risk | Mitigation |
|---|---|
| `Pack` struct size differs across compilers/configs | `#pragma pack(push,1)` enforced; assert sizeof(Pack) at compile time |
| Pump thread races with main thread on lockstep mutex | Existing `g_state.mutex` already guards; pump uses same lock |
| Game-time send/recv conflicts with lobby PING/PONG | Differentiate by `pack.type`; PING/PONG works connected or not |
| 15-frame stride feels laggy at 60fps (250ms input lag) | g_delay is tunable; default conservative, expose env var |
| Endianness mismatch (LE machines only matter for RUEEE compat) | Both x86, no issue |

## 6. Verification (post-build)

1. Two `loader.exe` instances on same machine (host + peer).
2. Both reach title screen at 60fps (Phase 6a confirms).
3. **NEW for 6b**: host log shows `peer connected at <addr>`, RTT logged.
4. Peer log mirror.
5. `Ctrl_Key` packets visible in log every 15 frames once stage starts
   (will require user pressing Z to start a game — title screen → stage).

## 7. Decision

**Adopt RUEEE wire format. Build pump infra. Wire input capture.**
Defer everything else to 6c-6g per phase6_plan.md.
