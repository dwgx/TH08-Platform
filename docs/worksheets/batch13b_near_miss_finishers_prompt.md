# Batch 13b — Near-Miss Finishers (≥85% functions)

**Project**: th08 decomp
**Working dir**: `D:\Project\THGlobal\th08-decomp`
**Karpathy applies**: surgical, no speculation, stop after 2 attempts per function.

---

## Why this batch

Batch 12 left `Gui::OnUpdate` at 90.32% — near-miss, 1 push from 100%. There are 58 functions in the 95-99% near-miss zone right now. Picking off near-misses gives the **highest perfect-count ROI per minute**: each is just a few bytes of codegen difference vs the original.

---

## Targets

### Tier 1 — Gui::OnUpdate at 90.32% (CRITICAL)

This is BLOCKING the Gui rendering chain. Push to 100%.

```bash
python ../tools/ida_helper.py call decompile '{"addr":"0x4338ca"}'   # Gui::OnUpdate
```

Look at what surrounding Gui functions do. Adjust the body to match ZUN's exact codegen.

### Tier 2 — top 10 near-misses ≥95%

Run this Python to get the list (do NOT include the AntiTamper wrappers AddLives/AddToBombCount which are stuck for known reasons):

```bash
python -c "
import json
d = json.load(open('diff_report.json'))
# Skip AntiTamper-wrapper functions
skip = ['AddLives','AddToBombCount','AddPower','AddScore','SetLives','SetPower']
near = sorted([f for f in d['data'] if 0.95 <= f.get('matching',0) < 1.0 and not any(s in f.get('name','') for s in skip)],
              key=lambda x: -x.get('matching',0))
for f in near[:15]:
    print(f'  {f.get(\"matching\",0)*100:.2f}%  size=?  {f[\"name\"]}')
"
```

Pick the top 10 (by current %). Attempt them.

---

## Workflow per function

Same as batches 8/9/10/11/12.

---

## Hard rules

1. GameManager `/Od /RTCu` — never touch.
2. VS2002: `time_t` / `time()`, no underscore-prefix.
3. **NO `u8 opaque[N]`** — see batch 9/10 history.
4. Don't change calling conventions without `mapping.csv` check.
5. Don't auto-launch th08.exe.
6. AntiTamper wrappers: skip.

---

## Stop conditions

- aixj 502 > 2× consecutive
- 25 min wall
- 5 wins reached
- 5 consecutive no-progress

---

## Return format

```json
{
  "gui_onupdate_after": "<float>",
  "tier_2_wins": <int>,
  "tier_2_win_list": ["th08::...", ...],
  "tier_2_fails": [{"name": "...", "prev_pct": "...", "new_pct": "...", "blocker": "..."}],
  "perfect_count_after": <int>,
  "stop_reason": "completed | api_failure | wall_time | no_progress"
}
```
