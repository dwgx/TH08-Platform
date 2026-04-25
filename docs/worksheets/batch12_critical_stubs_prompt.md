# Batch 12 — Critical OnUpdate Stubs (Codex Worker Prompt)

**Project**: th08 decompilation — attacking the HOT-PATH stubs
**Working dir**: `D:\Project\THGlobal\th08-decomp`
**Karpathy applies**: surgical, no speculation, no guess. Verify hex-rays before writing every line.

---

## Why this batch is high-value

The decomp is at 422/736 byte-perfect (57.34%). But **the recompile doesn't actually run** because every per-frame `*::OnUpdate` is a stub:

```cpp
ChainCallbackResult Player::OnUpdate(Player *player) { return CHAIN_CALLBACK_RESULT_CONTINUE; }
ChainCallbackResult EnemyManager::OnUpdate()         { return CHAIN_CALLBACK_RESULT_CONTINUE; }
ChainCallbackResult GameManager::OnUpdate(GameManager *gameManager) { return CHAIN_CALLBACK_RESULT_CONTINUE; }
// ...etc, 8 critical OnUpdate methods all stubbed
```

Filling these in unlocks **decomp playability** AND **DLL Phase 5 (2P)** — both routes are blocked on the same root cause.

---

## Targets (priority-ordered, big wins for the project)

### Tier 1 — Player class full body decomp (Phase 5 critical)

These are the **most valuable** stubs in the whole codebase. Player class is the target of P2 duplication for multiplayer.

| Function | Address | Size | Status |
|---|---|---|---|
| `Player::OnUpdate` | 0x44c390 | 0x211 (529B) | STUB |
| `Player::OnDrawHighPrio` | 0x44d530 | size unknown via mapping.csv (look it up) | STUB |
| `Player::AddedCallback` | 0x44d650 | 0x601 (1537B) | STUB |
| `Player::FUN_0044aec0` | 0x44aec0 | TBD | unnamed |
| `Player::FUN_0044c5b0` | 0x44c5b0 | TBD | unnamed |
| `Player::FUN_0044c650` | 0x44c650 | TBD | unnamed |
| `Player::FUN_0044cbf0` | 0x44cbf0 | TBD | unnamed |

For unnamed (`FUN_*`) functions, also try to **rename them** with descriptive names based on what hex-rays shows. Rename via mapping.csv if applicable.

### Tier 2 — Manager-class OnUpdate (game-loop critical)

| Function | Address | Notes |
|---|---|---|
| `GameManager::OnUpdate` | 0x439bc7 | size 0xe3c (3644B) — biggest, central dispatcher |
| `EnemyManager::OnUpdate` | 0x42c660 | enemies don't move without this |
| `BulletManager::OnUpdate` | 0x431240 | bullets don't move without this |
| `ItemManager::OnUpdate` | 0x440500 | items don't drift without this |
| `Gui::OnUpdate` | 0x4338ca | UI doesn't tick without this |
| `Background::OnUpdate` | 0x407400 | background animations |

### Tier 3 — Title / Result (boot-flow critical)

| Function | Address | Notes |
|---|---|---|
| `TitleScreen::OnUpdate` | 0x467399 | title screen menu logic |
| `ResultScreen::OnUpdate` | 0x4584b0 | end-of-stage screen |

### Stop after attempting Tier 1 + 1 from Tier 2 + 1 from Tier 3, OR 25 min wall-clock

You don't need to finish them all. **Player::OnUpdate alone is a massive win**.

---

## Workflow per function

1. Open `src/<Class>.cpp` — read existing stub.
2. `python ../tools/ida_helper.py call decompile '{"addr":"0x<addr>"}'` for full hex-rays (these are big — expect 100+ lines).
3. `python ../tools/ida_helper.py call analyze_function '{"addr":"0x<addr>"}'` — gives you struct field accesses, callees, callers, basic block layout.
4. **READ the surrounding code in the .cpp** — there are many already-perfect helper functions in the same file that show ZUN's idioms (variable naming, control flow style, struct access patterns).
5. Translate hex-rays to ZUN-style C++. Match:
   - Use `*(u32 *)0xADDR` for raw global writes (e.g. seen in `*(u32 *)0x160F42C = ...` style)
   - `g_GameManager.globals->score` for nested pointer access
   - `(u8 *)this + 0xN` for raw struct offset access (when field name unknown)
   - ZUN often uses `if (cond) { X; } else if (cond2) { Y; }` chains over switch
6. Build: `python scripts/build.py`
7. Diff: `cd build && reccmp-reccmp --target th08 --json ../diff_report.json`
8. Look up your function's `matching` ratio. **First target: get out of stub state (any nonzero match% > 0.5 is progress).** Reaching 100% is bonus.
9. **2 attempts max** per function then mark as fail. Don't fight ZUN's specific instruction selection if you're already at 90%+.

---

## Key rules — re-read before each edit

1. **GameManager codegen flag MUST stay `/Od /RTCu`** in `scripts/configure.py:100`. Never touch.
2. `__time32_t` / `_time32` do NOT exist in VS2002. Use `time_t` / `time()`.
3. **DO NOT add `u8 opaque[N]` fields to ANY struct.** This was tried in batches 9 and 10 and BOTH had to be reverted because BSS layout shifted, regressing 4+ unrelated functions. If you can't byte-match a struct's size, just leave it as-is; the orchestrator will handle struct sizes via DLL globals.h instead.
4. Don't change calling conventions without checking `mapping.csv`. ChainCallback static methods are `__fastcall`.
5. Don't commit game files (`.exe` / `.dat`). `.gitignore` should already exclude.
6. Don't modify `mapping.csv` unless a clear binary-vs-source signature mismatch is the root cause.
7. **Don't auto-launch th08.exe.** User has explicitly forbidden it.
8. **AntiTamper wrappers (AddLives, AddToBombCount, etc.) — DO NOT TOUCH** in this batch. They're stuck for known reasons (need `if (IsTampered()) CRASH_GAME();` rewrites). Different problem.

---

## Stop conditions

- aixj.vip 502 / SUBSCRIPTION_NOT_FOUND > 2× consecutive
- 25 min wall-clock
- Player::OnUpdate written + 1-2 manager OnUpdate written (= huge win)
- 5 consecutive function attempts failed to make progress

---

## Return format

End with **exactly** this JSON, no prose around it:

```json
{
  "tier_1_player": {
    "OnUpdate":           {"prev_pct": "0.00", "new_pct": "<float>", "wrote_lines": <int>, "blocker": "<str|null>"},
    "OnDrawHighPrio":     {"prev_pct": "0.00", "new_pct": "<float>", "wrote_lines": <int>, "blocker": "<str|null>"},
    "AddedCallback":      {"prev_pct": "0.00", "new_pct": "<float>", "wrote_lines": <int>, "blocker": "<str|null>"},
    "renamed_FUN_funcs":  ["...", "..."]
  },
  "tier_2_managers": [
    {"name": "th08::Foo::OnUpdate", "prev_pct": "0.00", "new_pct": "<float>", "wrote_lines": <int>, "blocker": "<str|null>"}
  ],
  "tier_3_screens": [
    {"name": "th08::Foo::OnUpdate", "prev_pct": "0.00", "new_pct": "<float>", "wrote_lines": <int>, "blocker": "<str|null>"}
  ],
  "perfect_count_after": <int>,
  "stop_reason": "completed | api_failure | wall_time | no_progress",
  "next_round_recommendation": "<one sentence on what's the highest-value next batch attempt>"
}
```
