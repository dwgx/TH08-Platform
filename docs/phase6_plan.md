# Phase 6 — Process-per-player Multiplayer (the real path)

> **Why Phase 6 exists**: Phase 5 (in-process 2P via `g_Player2` chain
> registration + dual_collision / item_routing / enemy_aim hooks) hit a
> structural perf ceiling at ~24-30fps. The cause is not any one hook —
> it is `Player::OnUpdate` running 2× per frame plus `BulletManager` /
> `EnemyManager` not being thread-safe / 2P-aware in vanilla ZUN code.
> **DLL injection cannot rewrite vanilla logic**. To get 60fps real 2P
> via DLL inject we have to stop pretending one process is two players.

## 1. Architecture

```
Player A's machine                     Player B's machine
┌───────────────────────┐              ┌───────────────────────┐
│ th08.exe (vanilla)    │              │ th08.exe (vanilla)    │
│ + th08_platform.dll   │ ←── UDP ──→  │ + th08_platform.dll   │
│ Phase 4 net layer     │              │ Phase 4 net layer     │
│                       │              │                       │
│ Local: A as P1        │              │ Local: B as P1        │
│ Remote echo: B sprite │              │ Remote echo: A sprite │
└───────────────────────┘              └───────────────────────┘

Each peer plays vanilla single-player at 60fps.
UDP exchanges:
  - shared RNG seed at game start (deterministic enemy / bullet patterns)
  - input packets every 15 frames (lockstep-style; like RUEEE)
  - player pos/state echo (for ghost rendering on remote screen)
  - score / lives / power for HUD comparison
```

**Co-op semantics**:
- Both peers see **the same boss / same bullets / same enemies** because
  they share an RNG seed and game state diverges minimally.
- Each peer fights **on their own screen** — bullets fired by A's P1
  damage enemies on A's screen, B's bullets damage B's enemies.
- Visually you see your partner's "ghost" so it feels co-op even though
  the two BulletManagers are independent.
- Grade is shared (sum of both peers' scores) so it has the *feel* of
  team play.

This is **not** "two players hitting the same boss simultaneously"
(that requires C / decomp fork). It is the closest 60fps thing DLL
injection can deliver.

## 2. What we keep from Phase 4 + 5

### Keep & extend

| File | Phase | Reuse plan |
|---|---|---|
| `dll/src/net/udp_socket.{cpp,h}` | 4 | Direct reuse (Phase 4 UDP foundation) |
| `dll/src/net/lockstep.{cpp,h}` | 4 | Direct reuse (frame-stride input batching) |
| `dll/src/net/rollback.{cpp,h}` | 4 | Reuse for desync recovery |
| `dll/src/net/fake_rtt.{cpp,h}` | 4 | Reuse for testing latency |
| `dll/src/net/crc32.h` | 4 | Reuse for checksum-based desync detection |
| `dll/src/state/frame_state.{cpp,h}` | 4 | Reuse for state capture / restore |
| `dll/src/state/heap_journal.{cpp,h}` | 4 | Reuse for heap allocation tracking |
| `dll/src/hooks/input.{cpp,h}` | 2 | Reuse — adapt to drive lockstep |
| `dll/src/hooks/game_loop.{cpp,h}` | 2 | Reuse — drive frame ticking |
| `dll/src/state/p2_lives.{cpp,h}` | 5f | **Concept** reuse (per-player lives tracking) — but each peer reads own vanilla |

### Move to `legacy_local2p/` (kept for reference, NOT compiled into Phase 6 build)

| File | Phase | Why deprecated |
|---|---|---|
| `dll/src/state/player2.{cpp,h}` | 5a | g_Player2 not needed — peer is rendered as ghost only |
| `dll/src/state/player2_hook.{cpp,h}` | 5b | Chain register of P2 not needed |
| `dll/src/state/dual_collision.{cpp,h}` | 5c/5i-A | Each peer's vanilla collision handles its own P1 |
| `dll/src/state/item_routing.{cpp,h}` | 5i-B | Same — vanilla item routing per peer |
| `dll/src/state/enemy_aim.{cpp,h}` | 5i-C/D | Same — vanilla aim per peer |
| `dll/src/state/p2_input.{cpp,h}` | 5d | Replaced by network-driven input mirror |
| `dll/src/state/p2_mirror.{cpp,h}` | 5h | Hardcoded-call mirrors irrelevant |
| `dll/src/state/hud.{cpp,h}` | 5g | Will rewrite for shared score / peer HUD |

`docs/worksheets/phase5i_routing_re_survey.md` stays in place as the
record of what Phase 5 learned about ZUN's routing internals — it is
gold for any future decomp-fork work (Path C).

## 3. Phase 6 sub-phases

### 6a — Loader changes (~1 day)

`th08_platform_loader.exe` learns two roles:

```bash
# host: bind UDP, wait for peer, then launch game
loader.exe --host --listen 7480 path/to/th08.exe

# peer: connect to host, launch game when ready
loader.exe --peer host_ip:7480 path/to/th08.exe
```

Existing `TH08_PLATFORM_PEER` / `TH08_PLATFORM_LISTEN` env-var path
already works (Phase 4 plumbing). Loader just sets these env vars
based on flags before injecting DLL.

### 6b — Lockstep input pipeline (~2 days)

Already half-built in `dll/src/net/lockstep.cpp`. Need:

1. Pack local-frame input bits and send to peer at frame-stride boundary
   (every 15 frames per RUEEE convention; drives 4 packets/sec @ 60fps).
   Reuse `Bits<16>` packing semantics from RUEEE's `Connection.hpp` for
   wire compatibility (could even talk to RUEEE peers later if we wanted).
2. Receive peer's input batch and apply for current peer-frame in
   `hooks/input.cpp` (we already hook `Controller::GetInput`).
3. Frame-pacing: if peer is behind, pause local game until peer catches
   up (vanilla game has frame-skip logic, leverage it). Existing
   `frame_state.cpp` rollback can recover from packet loss.

### 6c — Shared RNG seed (~0.5 day)

At connection establishment (handshake before game start), one peer
generates a 32-bit seed, sends to other. Both peers patch the seed
into ZUN's RNG init at the start of the run. Must verify:

- Where ZUN's stage RNG seeds come from (need quick IDA pass on RNG
  init path — `g_Rng` at `0x0164d520` already in `globals.h`).
- That patching the seed at game-start makes enemy spawns / bullet
  patterns deterministic across peers.

If determinism falters (audio rng, decoration rng), focus on the
gameplay-affecting paths only and accept cosmetic divergence.

### 6d — Peer ghost rendering (~2-3 days)

Render the remote player on the local screen as a translucent sprite.

- New file `dll/src/state/peer_ghost.cpp` (replaces `player2.cpp`).
- Hook a draw chain entry to draw a sprite at the peer's pos
  (received via UDP) once per frame. AnmManager sprite reuse from
  Player::AddedCallback texture (the spell card / costume sprite).
- No chain register, no OnUpdate, no input — just a draw element.
- Costs ~0.1ms per frame; doesn't push frame budget.

### 6e — HUD: shared score + peer status (~1 day)

Rewrite `dll/src/state/hud.cpp` to show:

- Local: lives / bombs / power / score (vanilla)
- Peer: lives / bombs / power / score (from UDP)
- Shared: combined score (sum) for that "team grade" feel
- Connection: latency / packet-loss indicator

### 6f — Match start / lobby (~2 days)

Pre-game flow:

1. Host launches `loader.exe --host`. Loader binds UDP and waits.
2. Peer launches `loader.exe --peer <host>`. Loader connects.
3. Host's DLL displays "Waiting for peer ready" text on title screen
   (hook AsciiManager::AddString to inject).
4. Peer's DLL signals "ready" via UDP.
5. Host displays "Starting in 3.. 2.. 1.." countdown, sends RNG seed +
   "go" signal. Both peers Push start key simultaneously (or one peer
   relays start to the other).

Optional later: external matchmaker (Phase 7).

### 6g — Desync detection / recovery (~1-2 days)

Use `dll/src/net/crc32.h` to checksum game state every N frames. If
peers' checksums diverge, trigger `Ctrl_Try_Resync`-style flow (RUEEE's
approach):

- Host snapshots current state via `frame_state::capture()` (existing).
- Sends to peer.
- Peer restores via `frame_state::restore()` (existing).
- Resume from common frame.

Phase 4 rollback skeleton handles this; just wire up the trigger.

## 4. Total Phase 6 estimate

~10-15 working days for a playable 6a-6g end-to-end MVP.
Breakdown:

| Sub-phase | Days |
|---|---|
| 6a loader | 1 |
| 6b lockstep | 2 |
| 6c RNG seed | 0.5 |
| 6d ghost render | 2-3 |
| 6e HUD | 1 |
| 6f match start | 2 |
| 6g desync | 1-2 |
| Integration / testing | 2 |

This pivot is **technically less work than continuing to fight
in-process 2P**, because most of the network plumbing already exists
in Phase 4 — Phase 5's hook-mirror-spoof pile of code is what gets
deleted (or moved to legacy/).

## 5. Build prerequisites

- Phase 4 DLL boots clean (verified across many sessions).
- `TH08_PLATFORM_PEER` / `TH08_PLATFORM_LISTEN` env vars wired.
- Two physical machines OR `127.0.0.1:7480` + `127.0.0.1:7481` for
  same-machine testing with two game windows.
- `loader.exe` must allow launching multiple game instances (verify
  that `loader.exe path/to/th08.exe` works when th08.exe is already
  running — may need a copy of th08.exe to a second path because of
  ZUN's mutex check; verify in 6a).

## 6. Risks / unknowns

| Risk | Mitigation |
|---|---|
| Two th08.exe instances may conflict on shared mutex / config files | Run from separate working dirs; copy th08.exe + cfg per peer; verify in 6a |
| RNG patching may not cover all paths (audio / decoration rng) | Accept cosmetic divergence; only sync gameplay-affecting paths |
| Lockstep latency makes 60fps perceived as 60-X (X = round-trip) | Use rollback to mask jitter; target same-LAN play first |
| `KeyPackFrameNum=15` from RUEEE may be too lax for 60fps action | Profile and reduce to 5-8 frames if needed |
| Vanilla th08 boundary checks may dislike same-machine multi-instance | Test on dedicated VMs / two PCs; document in 6a outcome |

## 7. Long-term: Path C (decomp fork) — **NOT** Phase 6 scope

If Path C is taken later (true 2P, both fight same boss):

- Wait until `th08-decomp` is byte-perfect (currently 414/736 = 56%).
  Estimate: 6+ months at current cadence.
- Once byte-perfect, fork th08-decomp as `th08_2p_native`.
- Modify `BulletManager`, `EnemyManager`, `Player`, `Chain` to natively
  support two players (similar to RUEEE's th06 fork).
- Replace lockstep peer-input sync with same-process two-Player update.
- Each "client" of platform is a separate th08_2p_native instance;
  matchmaking still routes peers together.

## 8. Decision

**Proceed with Phase 6 = Path B = process-per-player + UDP**. Path C
is a future option that doesn't block Phase 6 (Phase 6 ships value
now; Path C can replace or coexist later if the platform demands true
same-screen 2P).

Phase 5 work is preserved in `legacy_local2p/` and stays as decomp-fork
reference material for an eventual Path C.
