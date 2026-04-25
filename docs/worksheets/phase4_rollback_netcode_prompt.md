# Phase 4 — Rollback Netcode (Codex Worker Prompt — DRAFT)

**Status**: dispatch only AFTER Phase 3 verifies via inject test (two instances connect, exchange inputs, both log `synced frame N: local=0x.. remote=0x..`).

**Project**: th08_platform DLL multiplayer mod
**Working dir**: `D:\Project\THGlobal\TH08-Platform\dll`
**Karpathy applies**: surgical, no over-engineering, define verifiable milestones.

---

## Goal

Add **rollback netcode** on top of Phase 3 lockstep. The game predicts the peer's input each frame, advances the simulation, then on confirmed-input arrival either accepts the prediction (no rollback) or rewinds + replays. Phase 4 ends when:

1. Build succeeds.
2. Two instances on same machine produce a deterministic match across `~16` frames of artificial RTT injection (`TH08_PLATFORM_FAKE_RTT_MS=200`). Both replays show identical state at frame N+200ms.
3. Visual smoothness: no input-lag stutters even at 100ms simulated RTT (vs Phase 3 lockstep which would visibly stutter).
4. Desync detection: Phase 4 emits per-frame state checksums and logs `desync detected at frame N` if the two instances disagree, enabling debugging of any state-capture omission.

Do NOT implement: 2P player ship duplication, HUD changes, separate lives — those are Phase 5.

---

## Why rollback (vs lockstep)

**Phase 3 lockstep**: each frame waits up to 200ms for peer input. If RTT is 100ms, every frame has 100-200ms input lag. At 60 FPS that's 6-12 frames of visible delay. Players notice. WAN play is unplayable.

**Rollback (Phase 4)**: each frame predicts peer input (= last known input or repeat), advances simulation immediately, no waiting. When confirmed peer input arrives later (5-10 frames later for typical 100ms RTT), check if prediction matched: if yes, no-op; if no, rewind to confirmed-input frame, replay forward with correct input. From the player's perspective, gameplay is real-time even at 100ms RTT.

Rollback only works because **Phase 1 already gave us deterministic state capture+restore**.

---

## Design

### State ring buffer

```cpp
constexpr std::size_t kMaxRollbackFrames = 16;

struct RollbackBuffer {
    std::array<state::FrameState, kMaxRollbackFrames> history;
    // Latest confirmed-by-peer frame number
    std::uint64_t confirmed_frame = 0;
    // Latest local frame (= now)
    std::uint64_t local_frame = 0;
    // Last index in history written
    std::size_t head = 0;
};
```

Each frame:
1. **At hook entry of GameManager::OnUpdate** (before simulation runs):
   - `state::capture(history[head], local_frame)` — save current state
   - `head = (head + 1) % kMaxRollbackFrames`
2. Simulation runs normally (game's existing OnUpdate logic).
3. **At hook exit** (after simulation):
   - Compute `checksum = hash(history[head-1])`
   - Send to peer over UDP (extend Phase 3 packet to include 4-byte checksum)
   - Receive peer's checksum for this frame
   - If different and we have peer's confirmed input for an earlier frame: trigger rollback

### Rollback procedure

```cpp
void rollback_to(std::uint64_t target_frame) {
    // 1. Restore state from history at target_frame
    auto idx = (head + kMaxRollbackFrames - (local_frame - target_frame)) % kMaxRollbackFrames;
    state::restore(history[idx]);
    
    // 2. Replay forward from target_frame to local_frame, using
    //    confirmed peer inputs for past frames and predictions for future
    for (auto f = target_frame; f < local_frame; ++f) {
        // Override CurFrameInput with whichever input we have for frame f
        const u16 input = peer_input_for_frame(f).value_or(predict_peer_input(f));
        *globals::at<u16>(globals::kAddr_g_CurFrameInput) = input;
        
        // Re-run a single tick — simulation runs as if for frame f
        single_step_simulation();
        
        // Re-capture state at this frame
        state::capture(history[(idx + (f - target_frame) + 1) % kMaxRollbackFrames], f);
    }
}
```

### Single-step simulation

This is the trickiest part: how do we run "one frame" of game logic without the normal hook flow?

**Option A — recursion-friendly hook**: when rollback is in progress, call `g_original_OnUpdate(gm)` directly, bypassing our hook's capture+log paths. Need a thread-local "in-rollback" flag.

**Option B — direct call to lower-level routines**: identify the "calc chain" entrypoint and call it. Riskier, more invasive.

Pick **Option A**. Simpler, safer.

### Audio mute during rollback

When replaying frames, the original code calls `g_SoundPlayer.PlaySoundByIdx(...)` which would re-trigger audio. Mute by:
- Setting a thread-local `s_in_rollback = true`
- Hooking `SoundPlayer::PlaySoundByIdx` to no-op when flag set
- Reset flag after rollback complete

### Checksum

Cheap CRC32 of the FrameState's region buffers concatenated. Use `<cstdint>` + a 256-byte CRC32 table inlined.

```cpp
std::uint32_t checksum(const state::FrameState& s) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (const auto& region : s.regions) {
        for (auto byte : region.bytes) {
            crc = crc_table[(crc ^ byte) & 0xFF] ^ (crc >> 8);
        }
    }
    return ~crc;
}
```

### Desync detection

On checksum mismatch with peer at confirmed-frame F:
1. Log `desync detected at frame F: local_crc=0x... remote_crc=0x...`
2. **Don't rollback** — just log. We can't recover from desync; user needs to disconnect and rejoin.
3. Optionally dump full FrameState at F to a file for offline diff debugging.

### Fake-RTT injection (for testing)

Read env var `TH08_PLATFORM_FAKE_RTT_MS` at init. If set, every outgoing UDP packet gets queued with `now() + RTT_ms` deadline; the queue drainer sends them out when their deadline arrives. Lets us simulate 100/200/500ms RTT for local testing without hitting real networks.

---

## Module layout

Create:
```
dll/src/net/
├── rollback.h         RollbackBuffer struct + functions
├── rollback.cpp       state ring + rollback_to() + replay logic
├── crc32.h            tiny inline crc32 (256-byte table)
└── fake_rtt.{h,cpp}   queue + drainer for fake RTT testing
```

Modify:
```
dll/src/hooks/game_loop.cpp — capture into ring buffer; trigger rollback after peer-input arrival
dll/src/net/lockstep.cpp    — extend packet format to include 4-byte checksum
dll/src/main.cpp            — read TH08_PLATFORM_FAKE_RTT_MS env var
CMakeLists.txt              — add new files
```

---

## Verification protocol (user runs)

### Single-machine test

```cmd
:: Terminal A
set TH08_PLATFORM_LISTEN=7480
set TH08_PLATFORM_FAKE_RTT_MS=100
loader.exe th08.exe --peer 127.0.0.1:7481

:: Terminal B
set TH08_PLATFORM_LISTEN=7481
set TH08_PLATFORM_FAKE_RTT_MS=100
loader.exe th08.exe --peer 127.0.0.1:7480
```

Both `log_pid<N>.txt` files should show:
- `phase 4 ready`
- `peer connected at ...`
- `synced frame 60: local=0x0001 remote=0x0001`  (lockstep still works as fallback)
- Periodic `rollback at frame 65 (predicted=0x0000, actual=0x0001)` when prediction misses
- NO `desync detected` lines (means simulation is deterministic)

### Verifying determinism

Run two instances with `TH08_PLATFORM_FAKE_RTT_MS=100` for 30 seconds each. Their final logs' last 50 frame state-checksums should be identical between the two instances. (If not, we have a determinism leak — need to find what state isn't being captured.)

---

## Hard rules

1. **Don't break the existing Phase 1-3 hooks.** Phase 4 EXTENDS them, doesn't replace.
2. **Don't auto-launch th08.exe.**
3. **Don't add new dependencies.** Use only what we already link (Win32 API + ws2_32 + minhook + std).
4. **Don't implement P2 ship rendering / HUD.** That's Phase 5.

---

## Stop conditions

- aixj 502 > 2× consecutive
- 25 min wall
- Build keeps failing after 3 fix attempts on the same root cause
- Single-step simulation (Option A) doesn't work cleanly — escalate as open question

---

## Return format

```markdown
## Phase 4 — done

### Files created
### Files modified
### Design decisions
- Single-step approach used (Option A or B?)
- Checksum function chosen (CRC32 / xxhash / etc.)
- Audio mute mechanism

### Build result
### What user will see
- Exact log lines for handshake, sync, rollback, desync, timeout

### Open questions
- Audio mute interaction with sound looping
- HeapJournal: still stub from Phase 1 — when does it need filling?
- ECL VM execution under rollback — does ZUN's interpreter clobber any state we don't capture?
```
