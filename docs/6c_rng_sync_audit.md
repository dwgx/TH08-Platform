# Phase 6c — Architecture audit: shared RNG seed at game start

> Claude IDA work BEFORE codex dispatch. Per `feedback_architecture_first.md`.

## 1. RNG mechanics in TH08 v1.00d

Three layers:

| Address | Function | Role |
|---|---|---|
| `0x0164D520` | `g_Rng` (data) | 6-byte RNG struct: `uint16 state @ +0`, `uint32 counter @ +4` |
| `0x43ECC0` | `sub_43ECC0(this)` | 16-bit RNG step. Mutates `*(uint16*)this`, increments `*(uint32*)(this+4)`, returns new state |
| `0x43ED20` | `sub_43ED20(this)` | Combines two 16-bit calls into a 32-bit RNG word |
| `0x406EF0` | `sub_406EF0(this, max)` | Returns `RandRange(max)` = `sub_43ED20(this) % max` |

**Step formula** (decompiled from `sub_43ECC0`):

```c
__int16 sub_43ECC0(_DWORD *this) {
    *(_WORD *)this = ((((*(_WORD *)this ^ 0x9630) - 25939) & 0xC000) >> 14)
                   + 4 * ((*(_WORD *)this ^ 0x9630) - 25939);
    ++*(this + 1);  // 32-bit counter at offset +4
    return *(_WORD *)this;
}
```

**Determinism**: given same `state` value, sequence is fully deterministic.
The counter `+4` is just bookkeeping (visible to RNG users via different
field if they want it — not part of the state advance).

## 2. Sync strategy

### 2.1 Wire transport

Use existing `protocol::Ctrl_Set_InitSetting` (RUEEE pattern). Host
broadcasts seed in every periodic PING; peer captures the first one
it sees.

```
Host PING:
  pack.type                     = PACK_PING
  pack.ctrl.ctrl_type           = Ctrl_Set_InitSetting
  pack.ctrl.init_setting.delay  = uint32(host_seed)   // reuse `delay` slot
  pack.ctrl.init_setting.ver    = MULTI_NET_VER

Peer:
  on PACK_PING with Ctrl_Set_InitSetting:
    capture init_setting.delay → peer_received_seed
```

**Why reuse `delay`**: 6c doesn't have a delay-tuning UI yet (that's 6f).
The slot is currently unused on our side. When 6f introduces real delay
negotiation, we add a separate field or split the wire format.

### 2.2 Seed generation

Host generates a 32-bit seed at `initialize_listen_only()`:

```cpp
g_state.host_seed = static_cast<std::uint32_t>(::GetTickCount64() ^ ::GetCurrentProcessId());
```

Or, more deterministic for testing: `TH08_PLATFORM_SEED` env var (debug
override). Default = derived from time + PID.

### 2.3 Application moment — `frame == 1` of GameManager::OnUpdate

The existing `hooked_OnUpdate` in `dll/src/hooks/game_loop.cpp` fires
before the original `GameManager::OnUpdate`. We have:

```cpp
const auto f = g_frame_count.fetch_add(1, ...) + 1;
// ... existing rollback/state hooks ...
const int result = g_original_OnUpdate(gm);  // ZUN's stage script runs here
```

`f == 1` is the very first stage tick — BEFORE ZUN's stage script runs.
RNG-using calls (enemy spawns, bullet patterns) happen INSIDE that
stage script.

So we patch `g_Rng` at `f == 1` BEFORE calling original. Both peers
write the same seed → deterministic stage RNG.

### 2.4 What gets written

```cpp
auto* state_word   = reinterpret_cast<std::uint16_t*>(0x0164D520);
auto* counter_word = reinterpret_cast<std::uint32_t*>(0x0164D524);
*state_word   = static_cast<std::uint16_t>(seed & 0xFFFF);  // 16-bit seed
*counter_word = 0;                                          // reset bookkeeping
```

(Counter reset is cosmetic — it's a "calls-since-init" tally, not part
of the RNG mathematics. Keep it 0 anyway for clean state.)

## 3. Edge cases

| Case | Handling |
|---|---|
| Host starts game before peer connects | Host's `f==1` fires anyway, apply local seed; peer applies whatever seed arrives later when it joins (game already mid-stage on host → divergence; this is a 6f/lobby concern, not 6c) |
| Peer connects but never receives Ctrl_Set_InitSetting before its own f==1 | Peer falls back to local seed (its own ZUN-init seed); divergence. Mitigation: host PINGs at 250ms cadence so within 1s of connection peer has seed. f==1 only fires after stage-select, which always takes > 1s. |
| Connection drops mid-stage and reconnects | 6c only seeds at f==1; mid-stage reconnect doesn't re-seed. Acceptable for 6c. 6g / `Ctrl_Try_Resync` will handle this. |
| Host vs peer terminology — who generates? | Whoever has `is_host` flag set (`initialize_listen_only` path). In peer mode, `host_seed` field is unused; peer waits for incoming. |

## 4. Implementation files

| Action | File | LOC est |
|---|---|---|
| Modify | `dll/src/net/lockstep.cpp` — add `host_seed` / `peer_received_seed` fields, fold into PING ctrl, capture on recv | +50 |
| Modify | `dll/src/net/lockstep.h` — export `peek_shared_seed()` / `has_shared_seed()` | +10 |
| New | `dll/src/state/rng_sync.{cpp,h}` — write to `0x0164D520` (volatile, plain memory) | 60 |
| Modify | `dll/src/hooks/game_loop.cpp` — at `f == 1` call `state::rng_sync::apply_if_ready()` | +15 |
| Modify | `dll/CMakeLists.txt` | +1 |

**Est**: ~140 net LOC.

## 5. Verification (post-build)

1. Two `loader.exe` instances, host + peer (Phase 6a-b path).
2. Host log: `phase 6c: generated host seed = 0xXXXX`.
3. Peer log: `phase 6c: received shared seed = 0xXXXX` (matches host's).
4. After advancing past title → stage (user presses Z), both logs show
   `phase 6c: applied seed at frame 1: 0xXXXX`.
5. Stage 1 enemies / bullet patterns visually identical on both screens.
   (Fully deterministic given same seed and identical input — but our
   inputs aren't synced yet, so player movement will diverge. Enemy
   spawn timing should be locked.)

## 6. Risks

| Risk | Mitigation |
|---|---|
| ZUN re-seeds at random points (e.g., menu → stage transition writes a new seed) | Re-apply at `f == 1` overrides any ZUN reseed that ran in title screen |
| `0x0164D520` isn't really g_Rng | Verified: 13 readers in functions that look like enemy/bullet random pickers; counter at +4 incremented by RNG step. Confidence high. |
| Counter at `+4` matters for some RNG-derived value | Decompile shows it's just `++`; users read state word, not counter. Setting to 0 is safe. |
| Both peers have different seeds before f==1 | Host PINGs at 250ms; gap from "connected" to "f==1" is at least the title screen duration (seconds). Plenty of time. |

## 7. Out of scope (deferred)

- Per-frame RNG seed capture into `CtrlPack::rng_seed[15]` (6g desync detection).
- Lobby UI for delay negotiation (6f).
- Reconnect re-seed (6g).

## 8. Decision

**Patch `g_Rng @ 0x0164D520` at `f == 1` with shared 16-bit seed
delivered via `Ctrl_Set_InitSetting` PING.** Smallest change that
gets deterministic stage RNG.
