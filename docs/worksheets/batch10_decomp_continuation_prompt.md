# Batch 10 — Decomp Continuation (Codex Worker Prompt)

**Project**: th08 decompilation factory
**Working dir**: `D:\Project\THGlobal\th08-decomp`
**Karpathy applies**: surgical, no speculation, stop after 2 failed attempts per function.

---

## Context — what already happened

- Batch 9 codex hit aixj.vip rate limits and died mid-batch. Claude orchestrator manually resolved the Track A struct sizes (EnemyManager + BulletManager) for the DLL's `state/globals.h`, so **don't redo that**. Stay focused on Track B-style work.
- Codex's mid-batch flushed code (added `u8 opaque[0x9DCF10]` to EnemyManager.hpp etc.) was REVERTED because it regressed 4 functions by shifting BSS layout. **Don't reintroduce that pattern in this batch.** Adding huge `opaque[N]` fields to make `sizeof(T) == binary_size` causes the recompile's link-time BSS to grow, shifting other globals' addresses, breaking byte-match for unrelated code.

If you need to express "this struct is N bytes in the binary" without changing the recompile's BSS layout, use a non-allocating mechanism — e.g. `__declspec(align(...))`, `static_assert` on individual field offsets, or split-decomp (forward decl + manual bytes). But don't tackle this in batch 10 — defer to a focused investigation later.

## Goal

Push the byte-match perfect count from current `418 / 736` (56.79%) by attacking **AnmManager** and **ResultScreen** unmatched / near-miss functions.

Target: **5+ wins**. Stretch: 10. Stop when API rate-limits, 25 min wall-clock, or 5 consecutive no-progress attempts.

---

## Where to look

Worksheet `D:\Project\THGlobal\th08-decomp\worksheet_v8.md` lists 32 candidates including 10 ResultScreen + 4 AnmManager. The hex-rays section is missing per-function (the helper script's response parsing was buggy). **Fetch hex-rays + bytes per-function via `python ../tools/ida_helper.py call decompile '{"addr":"<addr>"}'`** — same pattern as batch 8's prompt.

Top opportunity (by current match% from `diff_report.json`):

### AnmManager (16 unmatched, 4 in worksheet_v8)
Pick small functions first. AnmManager has `kSize = 0x2a2570` (already C_ASSERTed in hpp, well-decomp'd). Look at `worksheet_v8.md` AnmManager section + `diff_report.json` for any AnmManager function with `matching > 0.85` and `size < 0x180`.

### ResultScreen (26 unmatched, 10 in worksheet_v8)
Many sit at 80-85% match — codegen-shape near-misses. Worksheet_v8 lists:
- `LinkScoreEx` 86.67%, `FreeScore` 85.71% (both forwarder functions; tricky)
- `MoveCursor`, `MoveShotTypeCursor`, `MoveCursorHorizontally` all at 80% (UI loops, easier)
- `FormatDate` 76% (uses `time_t/time()` — VS2002-friendly)
- `GetStageName` 60% (small lookup function — probably winnable)

Avoid:
- `LinkScoreEx` / `FreeScore`: batch 8 already failed these; they're forwarder-shape mismatches that need `#pragma var_order` or specific arg-passing hacks; defer.
- `HandleHighScoreScreen`, `CheckConfirmButton`, `DeletedCallback`: large/complex, low ROI.

---

## Workflow per function (same as batch 8 / 9)

1. Read existing `src/<Class>.cpp` + `<Class>.hpp` (often a stub body or partial impl).
2. `python ../tools/ida_helper.py call decompile '{"addr":"0x<addr>"}'` for hex-rays.
3. `python ../tools/ida_helper.py call get_bytes '{"regions":[{"addr":"0x<addr>","size":<size>}]}'` for original bytes (if useful).
4. Implement / fix the C++ body to match the hex-rays semantics + ZUN's idiomatic style.
5. `python scripts/build.py`.
6. `cd build && reccmp-reccmp --target th08 --json ../diff_report.json`.
7. Look up the function's `matching` ratio in the JSON. If `1.0`, win.
8. Move on. **2 attempts max per function** then mark as fail.

---

## Hard rules — re-read before edits

(Same as batch 9; copying for self-containment.)

1. GameManager codegen flag MUST stay `/Od /RTCu` (in `scripts/configure.py`). DO NOT touch.
2. `__time32_t` / `_time32` do NOT exist in VS2002. Use `time_t` / `time()`.
3. Don't change calling conventions without checking `mapping.csv`.
4. Don't commit game files. `.gitignore` should already exclude them.
5. Don't modify `reccmp-project.yml` or `mapping.csv` unless a clear binary-vs-source signature mismatch is the root cause.
6. **Don't add `u8 opaque[N]` fields to manager structs** — see context above. That pattern was tried in batch 9 and reverted.
7. Don't auto-launch th08.exe. The user has explicitly forbidden it.

---

## Stop conditions

Stop and emit JSON if any of these:

- aixj.vip returns 502 / SUBSCRIPTION_NOT_FOUND > 2× consecutive
- 25 min wall-clock elapsed
- 5 wins reached AND budget seems tight
- 5 consecutive function attempts failed to reach 100%

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
