# Phase 3 — UDP Lockstep Netcode (Codex Worker Prompt — DRAFT)

**Status**: dispatch only AFTER Phase 2 verifies via inject test (i.e., user has confirmed `phase 2 ready` + `input: cur=0x????` logs appear).

**Project**: th08_platform DLL multiplayer mod
**Working dir**: `D:\Project\THGlobal\TH08-Platform\dll`
**Karpathy applies**: surgical, simplicity, verifiable goals.

---

## Goal

First real "two-machine" multiplayer plumbing. **Lockstep** (not rollback yet — Phase 4) — each frame, both instances exchange inputs over UDP and proceed together. Phase 3 ends when:

1. Build succeeds with the new net code.
2. Two instances of `th08.exe` + DLL on the same machine (different ports) connect, exchange inputs, and both log:
   - `peer connected at <addr>:<port>`
   - Per frame: `synced frame N: local=0x???? remote=0x????`
3. The DLL does NOT alter game behavior — `g_CurFrameInput` is unchanged. We just OBSERVE peer's input to validate sync. Modifying gameplay = Phase 5.
4. Timeout / disconnect handling: if peer hasn't sent input within 200 ms, log `peer timeout` and continue with last known input (don't freeze).

---

## Why lockstep first, rollback (Phase 4) later

Lockstep is the simpler, deterministic-only baseline. We need it because:
- It validates the network plumbing without rollback's complexity (state save/restore is already Phase 1, but rollback adds re-simulation logic).
- LAN testing is straightforward: two machines, same subnet, no NAT.
- Rollback in Phase 4 adds prediction + reconciliation on top of this.
- If Phase 3 doesn't even sync, Phase 4 has no point.

---

## Design

### Packet format (16 bytes, fixed-width)

```
offset  size  field
0       4     magic = 'TH8L'   (0x4C385448, little-endian on disk)
4       1     version = 1
5       1     flags  (bit 0 = handshake, bit 1 = disconnect; rest reserved)
6       2     reserved (zero)
8       4     frame_number_lo  (frame % 2^32)
12      2     input_bitfield   (g_CurFrameInput value at send time)
14      2     reserved (zero)
```

Single packet per direction per frame. Total 16 bytes = ~960 bytes/sec at 60 fps. Comfortably within UDP budget.

### Module layout

```
dll/src/net/
├── udp_socket.h        wraps a UDP socket: open(local_port), send(addr,data), recv(timeout_ms) → optional<packet>
├── udp_socket.cpp      Winsock2 implementation
├── lockstep.h          send_local_input(frame, input); wait_for_peer_input(frame, timeout_ms) -> optional<input>
└── lockstep.cpp        glues udp_socket + protocol parsing
```

Modify:
- `src/main.cpp` — read `TH08_PLATFORM_PEER` env var; if set, init lockstep with peer addr; install the input hook only if lockstep init succeeds (otherwise stay observation-only)
- `src/hooks/input.cpp` — at hook entry, after the local read, send-then-wait via lockstep; log both inputs
- `CMakeLists.txt` — add net/*.cpp; `target_link_libraries(th08_platform PRIVATE minhook ws2_32)`

Do NOT modify:
- `src/state/*` (rollback territory, Phase 4)
- `loader/*`
- Other hook files

### Configuration

The loader already sets `TH08_PLATFORM_PEER` from `--peer ip:port` (see `loader/loader.cpp:99-104`). Phase 3 reads that env var.

Local port: hardcoded to `0` (kernel picks ephemeral) for outbound; bind a fixed local port from another env var `TH08_PLATFORM_LISTEN` (default `7480`). The user runs two instances on the same machine with:
- Instance A: `loader.exe th08.exe --peer 127.0.0.1:7481` (listens on 7480 by env default; sends to 7481)
- Instance B: `TH08_PLATFORM_LISTEN=7481 loader.exe th08.exe --peer 127.0.0.1:7480`

(Loader needs a small modification to forward `TH08_PLATFORM_LISTEN` if it's set in the parent env — this is built into how Windows passes env to child processes; CreateProcess inherits by default. Verify by reading `loader.cpp` around line 105.)

### Frame numbering

The OnUpdate hook in `src/hooks/game_loop.cpp` already maintains `g_frame_count`. Lockstep uses this same counter. Make `g_frame_count` accessible (move to a header, or expose via a getter).

### Send / wait logic

```cpp
// In hooked_GetInput or a new lockstep step called from there:
const auto frame = th08_platform::hooks::current_frame();   // expose g_frame_count
const auto local_input = *globals::at<u16>(globals::kAddr_g_CurFrameInput);

th08_platform::net::send_local_input(frame, local_input);

const auto peer_input = th08_platform::net::wait_for_peer_input(frame, /*timeout_ms=*/200);
if (peer_input.has_value()) {
    th08_platform::log_line("synced frame %llu: local=0x%04x remote=0x%04x",
        frame, local_input, *peer_input);
} else {
    th08_platform::log_line("peer timeout at frame %llu (local=0x%04x)", frame, local_input);
}

// Phase 3: do NOT modify g_CurFrameInput. That's Phase 5.
```

### Handshake

Before the first frame, both peers send a handshake packet (flags bit 0). Each side waits up to 5 sec for peer's handshake before declaring "peer connected" or "peer never appeared, running solo".

### Out-of-order / late packets

UDP can reorder. Mitigation:
- Each side keeps a `peer_inputs` map of `frame_number → input_value`.
- `wait_for_peer_input(frame)` polls the map first; if not present, recv until found or timeout.
- Old frames (frame_number < current - 10) are discarded.

### Disconnect

- If recv timeout 10× consecutively → log `peer disconnected, falling back to solo` and stop calling lockstep until a new handshake arrives.
- If a peer sends `flags & 2` (disconnect bit) → same.

---

## Verification protocol (the user runs)

This is what you write into the worksheet's "what user will see" section so they know how to verify after the build is clean.

### Single-machine local test (no real network)

Two instances of `loader.exe + th08.exe`, different listen ports, peer'd to each other:

```cmd
:: Terminal A
set TH08_PLATFORM_LISTEN=7480
loader.exe th08.exe --peer 127.0.0.1:7481

:: Terminal B (different terminal, different game install or with --multiprocess flag)
set TH08_PLATFORM_LISTEN=7481
loader.exe th08.exe --peer 127.0.0.1:7480
```

Both `%LOCALAPPDATA%\th08_platform\log.txt` (one per instance — they will OVERWRITE each other if they share the file. **TODO**: Phase 3 must use per-PID log filenames to avoid this.) should show:

```
phase 3 ready
peer connected at 127.0.0.1:7481  (or :7480)
synced frame 60: local=0x0000 remote=0x0000
synced frame 120: local=0x0001 remote=0x0000     <-- one player pressed Z
...
```

### Network conditions to test (defer to Phase 4 / actual play)

- Same-machine: trivial, just verifies plumbing
- LAN (two PCs): verify across real network stack
- WAN: high RTT + packet loss; lockstep will stutter (this is by design — rollback in Phase 4 hides it)

---

## Open design questions

1. **Per-PID log file**: the existing logger writes to a single fixed path. Two instances on one machine collide. **Suggestion**: append `_pid<N>.txt` to the filename when `TH08_PLATFORM_LISTEN` is set (i.e., we know we're in multiplayer mode). Keep single-file behavior for solo runs.
2. **Time source**: lockstep uses our hook's frame counter (g_frame_count), which increments per OnUpdate call. If a peer's machine has different OS-level frame timing, packet rate differs slightly. Lockstep is robust to this (each side waits for matching frame#). But pure-clock methods would not be.
3. **Checksum / desync detection**: not in Phase 3. Phase 4 (rollback) adds frame-state checksums.

---

## Build + verify checklist

1. `cmake --build build --config Release` succeeds.
2. `dumpbin /HEADERS` (or PE machine read) confirms 32-bit (i386).
3. `dumpbin /IMPORTS` shows `WS2_32.dll` in imports (confirms Winsock linked).
4. DO NOT run th08.exe. The user will run inject tests after they read your report.

---

## Return format

```markdown
## Phase 3 — done

### Files created
- src/net/udp_socket.{h,cpp}
- src/net/lockstep.{h,cpp}

### Files modified
- src/main.cpp
- src/hooks/input.cpp
- src/hooks/game_loop.h (expose current_frame())
- CMakeLists.txt
- (loader.cpp if needed for env-var forwarding)

### Protocol
- packet layout you settled on
- handshake design
- timeout values

### Build result

### What user will see
- exact log lines for handshake, sync, timeout, disconnect cases

### Open questions
- Per-PID log filename: did you implement this or defer?
- Any unexpected build issues / dependency surprises?
```
