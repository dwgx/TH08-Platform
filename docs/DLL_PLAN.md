# TH08-Platform DLL — Architecture Plan

## Goal

Ship `th08_platform.dll` + `th08_platform_loader.exe` that, when run against the original ZUN-released `th08.exe (1.00d)`, add 2-player co-op netplay with UDP rollback netcode. The original binary is never modified on disk.

## Why DLL injection, not a rewritten game

- Works on the actual retail binary — no redistribution of modified exe, no copyright drama
- Compatible with existing installs (Steam, DLsite, standalone)
- Uninstallable: don't run the loader → game runs vanilla
- Proven pattern: Giuroll (th12.3), thcrap (community translations)

The **decomp** in `game/` gives us *source-level understanding* — struct layouts, function signatures, field offsets. The DLL uses these as type definitions to hook the original binary at known addresses.

## Overall architecture

```
┌─────────────────────────────────────────────────┐
│ th08.exe (original 1.00d, unmodified on disk)    │
│                                                   │
│  ┌────────────────────────────────────────────┐  │
│  │ th08_platform.dll (injected at startup)     │  │
│  │                                              │  │
│  │  ┌────────┐   ┌──────────┐                 │  │
│  │  │ Hooks  │◀▶│ Netcode   │                 │  │
│  │  │ MinHook│   │ UDP       │                 │  │
│  │  └───┬────┘   └────┬─────┘                 │  │
│  │      │             │                         │  │
│  │  ┌───▼─────────────▼───┐                   │  │
│  │  │ State Capture        │                   │  │
│  │  │ (save/restore)       │                   │  │
│  │  └──────────┬──────────┘                   │  │
│  │             │                                │  │
│  │  ┌──────────▼──────────┐                   │  │
│  │  │ P2 Injector          │                   │  │
│  │  │ (second player)      │                   │  │
│  │  └─────────────────────┘                   │  │
│  │                                              │  │
│  │  ┌─────────────────┐                       │  │
│  │  │ ImGui overlay   │                       │  │
│  │  │ (lobby / debug) │                       │  │
│  │  └─────────────────┘                       │  │
│  └────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘
        ▲                             ▲
        │                             │
  loader.exe              External lobby / matchmaker
  (createproc + inject)   (Phase 6 — out of scope here)
```

## Hook points

Addresses resolved against `th08.exe v1.00d` base 0x400000; source-of-truth is `game/config/mapping.csv`.

| Hook | Address (file offset) | Purpose | First needed in |
|---|---|---|---|
| `th08::GameManager::OnUpdate` | 0x43aa03 | main frame tick — anchor for rollback | Phase 0 |
| `th08::GameManager::OnDraw` | 0x??? | rendering frame — ImGui overlay | Phase 1 |
| `th08::Controller::GetInput` | 0x??? | read / inject inputs | Phase 2 |
| `th08::Player::OnUpdate` | 0x??? | duplicate for P2 | Phase 5 |
| `th08::Player::OnDrawHighPrio` | 0x??? | draw P2 ship | Phase 5 |
| `th08::EnemyManager::OnUpdate` | 0x??? | state capture | Phase 1 |
| `th08::BulletManager::OnUpdate` | 0x??? | state capture | Phase 1 |
| `th08::Rng::GetRandomU32` | (already 100%) | seed sync | Phase 3 |
| `th08::GameManager::UpdateAntiTamper` | 0x406e50 | no-op replace | Phase 0 |

The `0x???` entries are filled in as the decomp factory byte-matches the named function — consult `game/config/mapping.csv`.

## Phases

### Phase 0 — Hello DLL *(current scaffold)*
Goal: prove the injection + hook + logging path works end-to-end.
- DllMain spawns an init thread (loader lock safe)
- Logger writes to `%LOCALAPPDATA%\th08_platform\log.txt`
- MinHook installs a detour on `GameManager::OnUpdate` @ 0x43aa03
- Detour increments a counter; every 60 frames, logs "frame N"

**Success = user runs loader, game starts, log file shows frame counter increasing**.

### Phase 1 — State capture / restore (1-2 weeks)
Required for any rollback work.

State to snapshot at frame boundary:
- `th08::GameManager` singleton (size 0x3de3c)
- `th08::ZunGlobals` via `GameManager::globals` (size 0xe4)
- `th08::Player` singleton
- `th08::EnemyManager` + its dynamic arrays
- `th08::BulletManager` + its dynamic arrays
- RNG state
- DirectInput input buffers

Implementation: at `OnUpdate` hook entry, memcpy tracked regions into a `FrameState` struct. Restore by reversing.

Gotcha: heap allocations within the frame must be tracked. Giuroll intercepts `HeapAlloc`/`HeapFree` and journals them per-frame for exact rollback.

### Phase 2 — Input hook (3-5 days)
- Hook `Controller::GetInput`
- Read local input, send over UDP
- Merge remote input into a `g_RemoteInput` buffer
- Phase 3 consumes this

### Phase 3 — UDP lockstep (1-2 weeks)
- UDP socket on configurable port
- Each frame: send local input + frame number; block until peer's matching-frame input arrives
- No rollback — deterministic but input-lag proportional to RTT
- Hard pauses on lost packets

**Success = two machines each running loader + DLL with `--peer <ip:port>` see each other's inputs in an overlay**.

### Phase 4 — Rollback netcode (3-4 weeks)
- Ring buffer of last N frame states (N = ~8-16)
- Predict input (repeat last) and advance
- On confirmed input: if prediction wrong, rollback + replay
- Desync detection (checksum per frame)

Reference: Giuroll's rollback module in Rust. We port the pattern to C++.

### Phase 5 — Second player (2-4 weeks)
The hardest phase. th08 is single-player at its core; we're teaching it co-op.

- Duplicate `Player` instance → `g_Player2`
- Extend hit detection, item attribution, graze counting per player
- Separate lives + bombs per player
- Shared stage progression (both players in same stage)
- HUD: P1 left, P2 right, shared score

### Phase 6 — Lobby / matchmaking (external, 1-2 weeks)
- Web or desktop app (separate from this repo potentially)
- Room listing, friend-invite, NAT traversal
- Hands off `--peer ip:port` to loader

## Design invariants

1. **Determinism is sacred.** Any non-deterministic call (timer, thread schedule, uninitialized memory) breaks rollback.
2. **State capture < 1 ms per frame** or we miss the 60 FPS budget.
3. **Decomp progress is optional for Phase 0-3.** Essential for Phase 5 (we need Player internals).
4. **Keep AntiTamper benign.** `UpdateAntiTamper` gets replaced with a no-op; `IsTampered` forced to return FALSE.

## Open questions

- **Audio on rollback**: each rollback rewinds state but replaying frames would retrigger sounds. Solution: mute during re-sim (Giuroll does this in `sound.rs`).
- **File I/O determinism**: replay recording, score writes. Intercept or defer.
- **DirectInput quirks**: held buttons vs edge triggers. Test during Phase 2.
- **Second player visual**: tint P1 sprite, or add new sprite sheet, or reuse reimu/marisa pairs? Design call in Phase 5.

## References

- [Giuroll](https://github.com/Giufin/giuroll) — Rust rollback mod for Touhou 12.3 Hisoutensoku
- [th06_multi_net](https://github.com/RUEEE/th06_multi_net) — th06 co-op via full decomp rewrite
- [GGPO](https://github.com/pond3r/ggpo) — canonical rollback SDK
- [MinHook](https://github.com/TsudaKageyu/minhook) — x86/x64 inline hooking
- [Dear ImGui](https://github.com/ocornut/imgui) — in-process overlay UI
