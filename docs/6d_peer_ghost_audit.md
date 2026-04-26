# Phase 6d.1 — Peer position transport (precursor to ghost rendering)

> Per `feedback_architecture_first.md`. Split 6d into two iterations:
> 6d.1 = position transport + log verification (this audit).
> 6d.2 = actual sprite render (separate audit, after 6d.1 lands).

## 1. Player position in TH08

From `memory/12_player_struct.md` (verified IDA, session 3):

| Address | Type | Name |
|---|---|---|
| `0x017D5EF8` | Player struct | `g_Player` |
| `0x017D5EF8 + 0x2B4` | f32 | `pos.x` (live world X, FUN_0044AEC0 writer) |
| `0x017D5EF8 + 0x2B8` | f32 | `pos.y` (live world Y) |

Concrete read at runtime:
```cpp
auto* g_player = reinterpret_cast<std::uint8_t*>(0x017D5EF8);
const float x = *reinterpret_cast<const float*>(g_player + 0x2B4);
const float y = *reinterpret_cast<const float*>(g_player + 0x2B8);
```

(The earlier `+0x3D4 / +0x3D8` we used in Phase 5a were SPAWN coords —
static, not live. Don't use those for ghost.)

## 2. Wire format

Reuse the existing Pack struct (152 bytes, RUEEE-compatible). Extend the
CtrlPack union with a new variant for ghost positions; this is a
**zero-byte change to sizeof(Pack)** because the union is already
sized by `Bits<16> keys[15]` (30 bytes).

```cpp
// protocol.h
enum Control : std::int32_t {
    ...
    Ctrl_Try_Resync      = 4,
    Ctrl_Ghost           = 5,   // NEW — position update for peer ghost
};

struct CtrlPack {
    ...
    union {
        Bits<16> keys[kKeyPackFrameNum];
        struct { std::int32_t delay; std::int32_t ver; } init_setting;
        struct { std::int32_t frame_to_re_sync; } resync_setting;
        struct { float pos_x; float pos_y; } ghost_pos;   // NEW
    };
    ...
};
```

`sizeof(Pack)` stays 152. RUEEE peers ignore `Ctrl_Ghost` (their default
switch case → no-op). Static_assert in lockstep.cpp continues to hold.

## 3. Send / receive flow

### Send
- Per frame, after the existing input capture in `hooked_GetInput`,
  read `g_Player.pos.x/y` and call `net::send_ghost_pack(frame, x, y)`.
- `send_ghost_pack`:
  - Build a Pack with `type=PACK_USUAL`, `ctrl_type=Ctrl_Ghost`,
    `frame=frame`, `ghost_pos = {x, y}`.
  - Send via existing socket.
- Sent at the GetInput cadence (60Hz when stage active, 0Hz at title).

### Receive
- Add `Ctrl_Ghost` case in `handle_packet_locked`:
  - Store `peer_ghost_x = pack.ctrl.ghost_pos.pos_x`,
    `peer_ghost_y = pack.ctrl.ghost_pos.pos_y`,
    `peer_ghost_frame = pack.ctrl.frame`.
- Throttled log at every 60 received ghost frames so we can confirm
  transport without log spam.

### Bandwidth
- 152 bytes × 60Hz = ~9.1 KB/s per peer outbound. LAN: trivial.
  Phase 6d.2 may switch to a smaller dedicated GhostPack (24B) if
  internet bandwidth becomes a concern.

## 4. New exports in `lockstep.h`

```cpp
void send_ghost_pack(std::uint64_t frame, float pos_x, float pos_y);
bool peek_peer_ghost(float& out_x, float& out_y, std::uint64_t& out_frame);
```

`peek_peer_ghost` returns `false` if no ghost packet received yet.

## 5. Hook integration

`dll/src/hooks/input.cpp::hooked_GetInput`, after the existing
`send_input_pack_if_due` call:

```cpp
// Phase 6d.1: stream local player position to peer for ghost render.
auto* gp = reinterpret_cast<const std::uint8_t*>(0x017D5EF8);
const float px = *reinterpret_cast<const float*>(gp + 0x2B4);
const float py = *reinterpret_cast<const float*>(gp + 0x2B8);
th08_platform::net::send_ghost_pack(frame, px, py);
```

`g_Player` is at a literal address — the same approach Phase 4/5 hooks
already use successfully (no ASLR for th08).

## 6. Verification (post-build)

1. Run host + peer (Phase 6a-c sequence).
2. Advance one peer past title to stage 1 (press Z).
3. That peer's log should show:
   - `phase 6d.1: sent ghost pos f=N x=192.0 y=384.0` (every 60 frames)
4. Other peer's log should show:
   - `phase 6d.1: received ghost pos f=N x=192.0 y=384.0` (every 60 frames)
5. Both peers in stage: positions diverge as each player moves
   (peer ghost log on each side reflects the OTHER's movement).

## 7. Out of scope (deferred to 6d.2)

- Actual visible sprite at peer's position
- AnmVm allocation for ghost
- Translucent / colored render
- Smoothing / interpolation between received frames

## 8. Implementation files

| Action | File | LOC est |
|---|---|---|
| Modify | `dll/src/net/protocol.h` (add `Ctrl_Ghost`, `ghost_pos` union variant) | +5 |
| Modify | `dll/src/net/lockstep.cpp` (state fields, send/recv, exports) | +60 |
| Modify | `dll/src/net/lockstep.h` (new exports) | +5 |
| Modify | `dll/src/hooks/input.cpp` (read g_Player, send ghost) | +10 |

Est: ~80 net LOC. Smallest possible cut for transport-only.
