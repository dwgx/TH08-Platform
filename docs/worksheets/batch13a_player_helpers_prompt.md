# Batch 13a — Player Helper FUN_ Implementations

**Project**: th08 decomp
**Working dir**: `D:\Project\THGlobal\th08-decomp`
**Karpathy applies**: surgical, no opaque[N], stop after 2 attempts.

---

## Why this batch

Batch 12 brought `Player::OnUpdate` from 0% (stub) → 81.78% (near-miss). Its remaining 18% gap is in helper callees that are still stubs. **Implementing these unlocks Player::OnUpdate at 100% (a huge milestone for Phase 5)**.

Codex 12's own recommendation:
> Highest-value next batch is to decompile Player helper bodies starting with FUN_0044c650 and FUN_0044d180, because Player::OnUpdate is now at 81.78% and its remaining gap is concentrated in those callees.

---

## Targets

Implement these Player helper functions (all called from Player::OnUpdate):

| Function | Address | Approach |
|---|---|---|
| `Player::FUN_0044c650` | 0x44c650 | Hex-Rays, translate to ZUN style |
| `Player::FUN_0044d180` | 0x44d180 | Hex-Rays, translate |
| `Player::FUN_0044d2c0` | 0x44d2c0 | Already at 66.67%; push to 100% |
| `Player::FUN_0044d420` | 0x44d420 | Hex-Rays, translate |
| `Player::FUN_00451150` | 0x451150 | Hex-Rays, translate |
| `Player::FUN_00451500` | 0x451500 | Already at 72.7%; push to 100% |
| `Player::FUN_0044aec0` | 0x44aec0 | Already at 33.3%; push higher |
| `Player::FUN_0044c5b0` | 0x44c5b0 | Already at 36.4%; push higher |
| `Player::FUN_0044cbf0` | 0x44cbf0 | Already at 30.8%; push higher |

After these, **rebuild + reccmp**. Player::OnUpdate should jump significantly. If close to 100%, push it the rest of the way.

**Bonus**: if you can rename the FUN_'s with descriptive names based on what they do (e.g. `UpdateMovement`, `HandleShoot`, `ProcessFocus`, `CalcCollision`), do so via mapping.csv updates. Make sure rename doesn't break the build.

---

## Workflow per function

Same as batch 12. `python ../tools/ida_helper.py call decompile '{"addr":"0x<addr>"}'`, translate, build, reccmp, attempt 2x then move on.

---

## Hard rules (re-read; non-compliance has cost real wins)

1. GameManager codegen flag MUST stay `/Od /RTCu` in `scripts/configure.py`. NEVER touch.
2. `__time32_t` / `_time32` do NOT exist in VS2002. Use `time_t` / `time()`.
3. **DO NOT add `u8 opaque[N]` fields to ANY struct.** Batches 9 and 10 did this and BOTH were reverted (BSS layout shift regresses 4+ unrelated functions). Batch 12 obeyed correctly. Continue obeying.
4. Don't change calling conventions without checking `mapping.csv`.
5. Don't auto-launch th08.exe.
6. AntiTamper wrappers: don't touch.

---

## Stop conditions

- aixj 502/SUB-NOT-FOUND > 2× consecutive
- 25 min wall-clock
- Player::OnUpdate reaches 100% (mission accomplished, stop)
- 5 consecutive function attempts fail to make progress

---

## Return format

```json
{
  "wins": <int>,
  "win_list": ["th08::Player::FUN_...", ...],
  "player_onupdate_after": "<float>",
  "fail_list": [{"name": "...", "prev_pct": "...", "new_pct": "...", "blocker": "..."}],
  "perfect_count_after": <int>,
  "stop_reason": "completed | api_failure | wall_time | no_progress | onupdate_100"
}
```
