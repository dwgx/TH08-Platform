# Batch 9 — Decomp Continuation (Codex Worker Prompt)

**Project**: th08 decompilation factory
**Working dir**: `D:\Project\THGlobal\th08-decomp`
**Karpathy applies**: surgical, no speculation, stop after 2 failed attempts per function.

---

## Goal

Two tracks in one batch:

### Track A — Resolve 2 of 4 TBD struct sizes (high priority for DLL Phase 1)

The DLL captures the live state of `th08::EnemyManager` and `th08::BulletManager` per frame, but their sizes are TBD because the decomp's `EnemyManager.hpp` / `BulletManager.hpp` only declare methods, no fields. **Determine each struct's actual size** from the binary, add `C_ASSERT(sizeof(T) == 0xN)` to the hpp, and (if you can) infer some of the field layout.

Inputs you have:
- `g_EnemyManager @ 0x00577f20`, next named global is `g_EnemyManagerCalcChain @ 0xf54e30` → upper-bound size 0x9DCF10 (~10 MB, likely correct because EnemyManager owns big bullet/enemy arrays).
- `g_BulletManager @ 0x00f54e90`, next named global is `g_BulletManagerCalcChain @ 0x160f408` → upper-bound size 0x6BA578 (~7 MB).

Workflow:
1. Use `python ../tools/ida_helper.py call analyze_function '{"addr":"<func>"}'` on functions in `EnemyManager` and `BulletManager` modules to find max-offset member accesses.
2. Use `xrefs_to '{"addrs":["0x577f20"]}'` to see what code touches `g_EnemyManager` directly (often constructor / Initialize is most informative).
3. Read upstream sister projects (`th06`, `th07`, `th09` decomps) if any field layout has been documented. (Don't blindly copy — th08 has its own struct layout.)
4. Decide: if the upper-bound matches a clean power-of-two-aligned plausible struct size, accept it as the `sizeof`. If the binary clearly has padding or anonymous sub-structs, document the uncertainty in a comment.
5. Write `C_ASSERT(sizeof(EnemyManager) == 0x...)` and `C_ASSERT(sizeof(BulletManager) == 0x...)` in their hpps. The build must still pass with these new asserts (i.e., the existing field layout matches).
6. **Do not invent struct fields**. Only assert the known size.

Output for Track A: 2 modified hpp files + 0-2 explanatory hpp comments. The DLL's `state/globals.h` should NOT be modified by you — the orchestrator (Claude) updates it after this batch lands. Just leave the C_ASSERTs.

### Track B — AnmManager near-miss push (perfect-count gains)

16 unmatched AnmManager functions exist (per the latest reccmp). Worksheet v8 already lists 4 candidates. Try to push 3-5 of them to 100%.

Workflow per function: same as batch 8 (read existing src, fetch hex-rays, implement, build, reccmp).

---

## Stop conditions

Stop and emit the JSON return whenever any of these:

- aixj.vip returns 502 or SUBSCRIPTION_NOT_FOUND more than 2× consecutively
- 25 minutes wall-clock elapsed
- Track A complete + 5 AnmManager wins (call it a successful batch and stop)
- 5 consecutive function attempts failed to reach 100%

---

## Hard rules — same as batch 8

1. GameManager codegen flag MUST stay `/Od /RTCu` (in `scripts/configure.py`).
2. `__time32_t` / `_time32` do NOT exist in VS2002 CRT. Use `time_t` / `time()`.
3. Don't change calling conventions without checking mapping.csv.
4. Don't commit game files. `.gitignore` should already block them.
5. Don't modify reccmp config or mapping.csv unless a clear binary-vs-source signature mismatch is the root cause of a fail. Document any such change in the commit message.
6. Don't fight AntiTamper-wrapper functions (AddLives, AddToBombCount). They're stuck at 70-78% for known reasons; rewriting their bodies is in scope but optional, not the priority for this batch.

Per the user's `feedback_orchestrator.md`: do NOT auto-launch th08.exe. Reccmp + build only.

---

## Return format

End with **exactly** this JSON, no prose around it:

```json
{
  "track_a": {
    "EnemyManager": {"size": "0x...", "confidence": "high|medium|low", "reasoning": "<one line>"},
    "BulletManager": {"size": "0x...", "confidence": "high|medium|low", "reasoning": "<one line>"}
  },
  "track_b": {
    "attempted": <int>,
    "wins": <int>,
    "win_list": ["th08::AnmManager::...", ...],
    "fails": [{"name": "...", "current_pct": "<float>", "previous_pct": "<float>", "blocker": "<short>"}]
  },
  "perfect_count_after": <int>,
  "stop_reason": "completed | api_failure | wall_time | no_progress"
}
```

If Track A failed (couldn't determine a confident size), report `{"size": "unknown", "confidence": "low", "reasoning": "..."}` instead of guessing. Honest uncertainty > false precision.
