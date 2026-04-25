# Batch 11 — Decomp Continuation (Codex Worker Prompt)

**Project**: th08 decompilation factory
**Working dir**: `D:\Project\THGlobal\th08-decomp`
**Karpathy applies**: surgical, no speculation, stop after 2 failed attempts per function.

---

## Current state (verify first via `cd build && reccmp-reccmp --target th08 --json ../diff_report.json`)

- 421 / 736 perfect (57.20%) as of batch 10's 3 wins
- ResultScreen: 23 unmatched (after batch 10's 3 cursor wins)
- Supervisor: 20 unmatched
- AnmManager: 16 unmatched (DrawVmTextFmt at 91.43% from batch 10's partial work)
- ScoreDat: 10 unmatched
- AsciiManager / Ending / Background / EnemyManager: ≤ 8 each

## Goal

Push perfect count higher. Target: **5+ wins**, stretch 10. Stop on rate-limit, 25 min wall, 5 consecutive no-progress, or 5+ wins reached.

---

## Targets (priority-ordered)

### High-value (small + winnable)

1. **AnmManager small fns** — Worksheet `worksheet_v8.md` listed `ClearVertexBuffer` etc but those are already perfect. Find unmatched AnmManager fns with `size < 0x100` via `diff_report.json`. AnmManager.cpp already has batch 10's local-variable refactor of `LoadExternalTextureData` — codegen patterns might transfer.
2. **AnmManager::DrawVmTextFmt** — 91.43% (varargs stack frame 0x94 vs original 0x8c). Try `#pragma var_order` annotations, see `BatchAddSprite` / similar already-perfect funcs in AnmManager.cpp for working patterns.
3. **Supervisor unmatched (20)** — many already at 95%+. Sort by current match% descending in `diff_report.json` and attack the top 5.
4. **ScoreDat fns** — fresh territory, simple I/O code, often easy first attempt.

### Avoid (cost > benefit per batch 10 evidence)

- `ResultScreen::FormatDate` — `time_t/_time32` shape mismatch, batch 10 couldn't fix
- `ResultScreen::LinkScoreEx` / `FreeScore` — forwarder address-arithmetic mismatch, batch 8 + 10 both failed
- `ResultScreen::GetStageName` — branch/local-return shape, batch 8 + 10 both failed
- AntiTamper-wrapper functions (AddLives, AddToBombCount, etc.) — known stuck at 70-78%; require body rewrites including `if (IsTampered()) CRASH_GAME();` branches
- Anything `> 0x300` size — slow per-attempt, unlikely 100% on first iteration

---

## Workflow per function (same as batches 8/9/10)

1. Read existing `src/<Class>.cpp` and `<Class>.hpp`.
2. `python ../tools/ida_helper.py call decompile '{"addr":"0x<addr>"}'` for hex-rays.
3. `python ../tools/ida_helper.py call get_bytes '{"regions":[{"addr":"0x<addr>","size":<size>}]}'` for bytes.
4. Implement / fix C++ body; match hex-rays semantics + ZUN's idiomatic style.
5. `python scripts/build.py`.
6. `cd build && reccmp-reccmp --target th08 --json ../diff_report.json`.
7. Look up function's `matching` in JSON. `1.0` = win.
8. **2 attempts max** per function then mark fail.

---

## Hard rules (re-read; non-compliance has cost real wins)

1. GameManager codegen flag MUST stay `/Od /RTCu` in `scripts/configure.py`. NEVER touch.
2. `__time32_t` / `_time32` do NOT exist in VS2002. Use `time_t` / `time()`.
3. Don't change calling conventions without checking `mapping.csv`.
4. Don't commit game files (`.exe` / `.dat`).
5. Don't modify `reccmp-project.yml` or `mapping.csv` unless clear binary-vs-source signature mismatch.
6. **DO NOT add `u8 opaque[N]` fields to `EnemyManager` / `BulletManager` / any other large-binary-but-empty-decomp manager struct.** Doing so shifts BSS layout, regressing 4+ unrelated functions. Batches 9 AND 10 both attempted this and BOTH had to be reverted. The "we should have C_ASSERT(sizeof(T) == binary_size)" is a known goal but the `opaque[N]` mechanism is wrong; defer to a focused investigation later (will likely need linker hints, not padding fields).
7. Don't auto-launch th08.exe.

---

## Stop conditions

- aixj.vip 502 / SUBSCRIPTION_NOT_FOUND > 2× consecutive
- 25 min wall-clock
- 5 wins reached AND budget seems tight
- 5 consecutive function attempts failed

---

## Return format

End with **exactly** this JSON, no prose around it:

```json
{
  "attempted": <int>,
  "wins": <int>,
  "win_list": ["th08::Class::Func", ...],
  "fails": [{"name": "th08::Class::Func", "current_pct": "<float>", "previous_pct": "<float>", "blocker": "<short reason>"}],
  "perfect_count_after": <int>,
  "stop_reason": "completed | api_failure | wall_time | no_progress"
}
```
