# Batch 8 — Decomp Continuation (Codex Worker Prompt)

**Project**: th08 decompilation factory (separate from the DLL)
**Working dir**: `D:\Project\THGlobal\th08-decomp`
**Reference worksheet**: `D:\Project\THGlobal\th08-decomp\worksheet_v8.md` (32 candidates: ResultScreen×10, Supervisor×8, ScoreDat×6, AnmManager×4, ScreenEffect×4, sorted near-miss-first within each)

**Karpathy guideline applies**: think before coding, surgical changes, no speculation. **Stop fighting a function after 2 failed attempts** — diminishing returns are real.

---

## Goal

Push the byte-match perfect count from `414 / 736` higher. Each function that goes from `<99.99%` to `100%` (call it `1.0` matching ratio) is a win.

Target: 5+ wins in this batch. Stretch: 10. Don't burn the whole budget — if the API starts returning 502s, stop and report partial state.

---

## Workflow per function

For each entry in `worksheet_v8.md` (work top-down within each module — they're sorted near-miss-first):

1. **Read what we already have**:
   ```
   src/<Class>.cpp    — current C++ body (often a stub)
   src/<Class>.hpp    — struct definitions, method signatures
   config/mapping.csv — confirm address + calling convention
   ```

2. **Pull IDA Hex-Rays for the function**:
   ```bash
   python ../tools/ida_helper.py call decompile '{"addr":"0x<addr>"}'
   ```
   (Hex-Rays output may have C-isms that don't directly compile; you translate the *semantics*.)

3. **Pull original bytes for diff sanity-checking**:
   ```bash
   python ../tools/ida_helper.py call get_bytes '{"regions":[{"addr":"0x<addr>","size":<size>}]}'
   ```

4. **Implement the C++ body** matching Hex-Rays semantics in `src/<Class>.cpp`. Match ZUN's idiomatic style: explicit casts, `*(u32 *)0xADDR = ...` for raw global writes, `g_GameManager.globals->...` for nested member access, etc.

5. **Build**:
   ```bash
   python scripts/build.py
   ```

6. **Diff**:
   ```bash
   cd build && reccmp-reccmp --target th08 --json ../diff_report.json
   ```

7. **Check the function's match ratio** in `diff_report.json` → `data[]` where `name == "th08::Class::Func"`. If `matching == 1.0`, it's a win.

8. **Commit on win** (in th08-decomp repo):
   ```bash
   git add -p src/<Class>.cpp
   git commit -m "<class>::<func> byte-matched (<old_pct>% → 100%)"
   ```

9. **Move on**. If 2 attempts didn't reach 100%, log it as a fail and skip.

---

## Hard rules — DO NOT VIOLATE

These come from `D:\Project\THGlobal\CLAUDE.md` and `memory/05_gotchas.md`. Past sessions paid for these in wasted hours.

1. **GameManager codegen flag MUST stay `/Od /RTCu`**, not `/Os`. Search `scripts/configure.py` for `GameManager` to see the override. Touching it back to `/Os` regresses ~25 already-matched functions including `SetLives`.

2. **`__time32_t` / `_time32` do NOT exist in VS2002 CRT**. Use `time_t` and `time(&t)` instead. ResultScreen.cpp already does this — preserve.

3. **Don't introduce `extern "C"` or change calling conventions** without checking mapping.csv first. ZUN's static class methods that act as ChainCallback callbacks are `__fastcall`, not `__cdecl`.

4. **Don't commit game files** — `.gitignore` should already block `.exe / .dat`, but verify with `git status` before commit.

5. **Don't modify the reccmp config** (`reccmp-project.yml`, `mapping.csv`) unless a clear signature mismatch is the root cause of a fail. Document any change in the commit message.

6. **AntiTamper wrappers (AddLives, AddToBombCount, etc.) are stuck at ~70-78%**: their bodies need rewriting to include the `if (IsTampered()) CRASH_GAME();` branch. Move IsTampered out-of-line is **not** sufficient. If you tackle these, rewrite the wrapper bodies; otherwise leave them.

---

## Stop conditions

Stop the batch and return whatever you have if any of these:

- API returns `502 Bad Gateway` or `SUBSCRIPTION_NOT_FOUND` more than twice
- A build error keeps cascading after 3 fix attempts (something deeper is wrong; let the orchestrator triage)
- 30 minutes wall-clock elapsed
- 5 consecutive function attempts failed to reach 100%

---

## Return format

End with **exactly** this JSON, no prose around it:

```json
{
  "attempted": <int>,
  "wins": <int>,
  "win_list": ["th08::Class::Func", ...],
  "fails": [
    {"name": "th08::Class::Func", "current_pct": "<float>", "previous_pct": "<float>", "blocker": "<short reason>"}
  ],
  "perfect_count_after": <int>,
  "stop_reason": "completed | api_failure | repeated_build_errors | wall_time | no_progress"
}
```

The orchestrator (Claude) parses this JSON to decide next steps.
