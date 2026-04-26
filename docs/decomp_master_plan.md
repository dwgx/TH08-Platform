# Decomp Master Plan — TH08 1.00d → playable + RUEEE-style multiplayer fork

**Author**: Claude (orchestrator)
**Date**: 2026-04-26
**End goal**: a `th08-decomp` codebase complete enough to build a th08.exe
that's behaviorally identical to ZUN's, then fork it as `th08-multinet`
with the same architecture RUEEE used on TH06.
**Estimated total**: 3-4 months sustained work, ~50-70 codex batches,
moderate IDA + Claude orchestration time.

---

## 0. Where we are (baseline 2026-04-26 19:59)

```
diff_report.json (after batch 11 surgical commit caf9fa4):
  Total functions:    748
  Perfect (=1.00):    427  (57.1%)
  Near-miss (.95-1):   59  (mostly linkage/codegen, "permanent partial" turf)
  Medium (.50-.95):   148  (source-edit territory)
  Low (0-.50):        107  (full body rewrite needed)
  Zero (=0):            7  (stubs)
```

**Critical-path completion is ONLY 43.9% perfect.** The modules that
matter for gameplay (Player, EnemyManager, BulletManager, ItemManager,
GameManager, Item, Background, ResultScreen + AnmManager + AsciiManager)
have 212 functions, only 93 are byte-perfect. The rest are mostly
stubs or partial implementations.

The decomp **does** build a runnable `th08.exe` (~607 KB vs ZUN's
841 KB) — we're missing ~28% of the binary by size. That ~234 KB
deficit is mostly stage / enemy / bullet / item / player logic.

---

## 1. Goal definition (be explicit about what "done" means)

We aren't pursuing **100% byte-match** as the success criterion. That's
unbounded perfectionism — the linkage near-miss group alone (59 fns)
proves you can have functionally-identical code with non-identical
bytes due to compiler / linker flags. Instead:

| Criterion | How measured | Target |
|---|---|---|
| **Playability** | Boot, navigate menus, play Stage 1 through end credits, replay records and replays | All work |
| **Determinism** | Two builds with same input + seed produce identical state | CRC32 of GameState struct matches |
| **Behavioral fidelity** | Enemy spawn timing, bullet patterns, scoring all match vanilla within ±1 frame | Replay file from vanilla plays correctly on our binary |
| **Byte-match level** | reccmp avg accuracy across th08:: namespace functions | ≥85% (currently ~78%) |
| **Functional implementation rate** | Non-stub functions in critical-path modules | ≥95% (currently 43.9% perfect, but many partials are functional) |

The "byte match" metric is **secondary**. The primary metric is "can we
play the game from our binary identically to vanilla."

---

## 2. Phase plan (5 phases, ~3-4 months)

### Phase α — Settle the near-miss tier (1 week, low-priority)
Resolve the 59 near-miss (≥95%) status: either close them or formally
mark as "permanent partial" in `config/permanent_partials.csv` (new
file). This stops codex from re-attempting them in future batches and
keeps the metric honest.

**Tactic**:
1. For each near-miss, classify the diff:
   - `linkage`: import name decoration, CRT version, EH model
     differences. Mark permanent_partial unless binary patcher
     (`config/binary_patches.json`) can byte-substitute them.
   - `codegen`: register allocation, constant pool layout, branch
     direction. Try one careful source-side edit; if no-op, mark.
   - `data`: string literal mismatch, magic number, etc. Almost
     always closeable.
2. Schedule ONE binary-patcher batch (codex) that picks N
   `linkage`-classified .obj files and inserts patch entries to
   make the link output match. Precedent: `0ce3ac8` already proved
   this works (+19.6 pp on AddLives + AddToBombCount).

**Exit criteria**: every near-miss is either 100% or has an entry in
permanent_partials.csv (with one-line reason). No near-miss left
unaddressed.

**Estimated wins**: +15-25 perfect via binary patcher; the rest are
permanent partials.

### Phase β — Critical-path source implementation push (8-12 weeks, BULK OF EFFORT)

The 107 low-match (<50%) + 7 zero-match functions are mostly STUBS in
the decomp. They need full bodies written from scratch using IDA
Hex-Rays as ground truth.

Split by module priority for full game playability:

| Module | Functions to fix | Priority | Why critical |
|---|---|---|---|
| **Player** | 17 fns, 12 below 50% | P0 | Movement, shoot, bomb, death — game can't run without this |
| **GameManager** | 55 fns, 13 below 50% | P0 | Score, lives, bombs, gauge, anti-tamper, frame state |
| **EnemyManager** | 9 fns, 5 below 50% | P0 | Spawn, draw, hit detection — no stage without this |
| **BulletManager** | 2 fns tracked, may be incomplete catalog | P0 | Bullet sim, collision, render |
| **ItemManager** | 9 fns, 4 below 50% | P0 | P-points, life items, bombs, time orbs |
| **Item** | 7 fns, 5 below 50% | P0 | Per-item collect logic |
| **Background** | 9 fns, 7 below 50% | P1 | Scrolling, layer compositing — game runs without it but no scenery |
| **ResultScreen** | 29 fns, 18 below 50% | P1 | End-of-stage / end-of-game display |
| **EclManager** | (need to check) | P0 | Stage script VM — drives the entire stage timeline |
| **Stage / EnemyEclInstr** | (need to check) | P0 | Per-stage script execution |

**Workflow per function** (scripted via batch_analyze + codex):

```
1. Pre-analysis (Claude side, runs IDA MCP):
   - python tools/batch_analyze*.py <addr1>,<addr2>,... > worksheets/wsN.md
   - For each addr: pull Hex-Rays decompilation, list of callees,
     basic blocks, struct field references
2. Codex dispatch:
   - feed worksheet
   - codex writes the C++ body, edits header for any new struct fields,
     iteratively builds + reccmps
   - emit FINAL_STATS with delta
3. Claude verifies:
   - Read codex's diff
   - Surgical revert if regressions appeared (per batch 11 lesson)
   - Commit wins to th08-decomp git
4. Repeat for next 10-30 fns
```

**Estimated cadence**:
- 25-30 functions per worksheet (sweet spot per memory/03_factory_pipeline)
- 10-15 wins per worksheet (proven rate from prior batches)
- 1 worksheet per day = ~10-15 wins per day
- 100-120 wins per week of sustained work
- 350-500 functions in 8-12 weeks → covers all critical-path

**Exit criteria**: every critical-path module is ≥95% perfect AND
behavioral spot-checks pass (replay file from vanilla TH08 plays
correctly on our build for the first 60 seconds of Stage 1).

### Phase γ — Stage / ECL VM completion (1-2 weeks)

The Stage Script VM is the heart of TH08 gameplay. Each stage is
defined by an `.ecl` file (compiled from ECL source) that the VM
executes frame-by-frame to spawn enemies, set music, run boss
patterns, dialog, etc.

**Subtasks**:
1. Verify `EclManager.cpp` and `EnemyEclInstr.cpp` coverage.
2. Implement the opcode dispatch (TH08's ECL has ~80 opcodes — see
   `Priw8/eclmap` for TH08's mnemonic file).
3. Hook EclManager to GameManager OnUpdate so stage scripts tick.
4. Verify with stage-1 replay file from vanilla.

**Exit criteria**: `replay_file.rpy` from vanilla plays correctly on
our binary for entire Stage 1.

### Phase δ — Full-game playability test + bug bash (1-2 weeks)

Run our built th08.exe against vanilla, comparing:
1. Boot, character select, stage select.
2. Stage 1-6 walkthroughs (no-miss replays).
3. Replay record / playback.
4. End-of-game scoring.
5. Practice mode, Spell Practice mode.
6. Last Word, Extra stage.

Expected divergences: timing-by-1-frame issues, RNG ordering, FPU
precision (single vs double in some places), z-order rendering bugs.
Each becomes a sub-batch for codex/claude.

**Exit criteria**: vanilla replay file completes Stage 6 boss correctly
on our binary with same final score.

### Phase ε — Multiplayer fork (2-3 weeks)

Branch as `th08-multinet`. Apply RUEEE-pattern modifications:

1. **Connection layer**: port `dll/src/net/lockstep.cpp` etc. into
   the decomp source as `Connection.cpp` (already RUEEE-format-
   compatible — same `Pack` struct, `CtrlPack`, `kKeyPackFrameNum`).
2. **Controller** modifications:
   - Add `key_2P_*` keybinds.
   - Add `GetInput_Net(frame, ...)` that combines local + peer keys
     with `g_delay` frame delay (RUEEE Controller.cpp:733-808).
   - Asymmetric input split: host's keys to P1 slot, guest's to
     P2 slot, both peers see same world.
3. **Player** changes:
   - Globally allow Player to be instantiated as P1 or P2.
   - Or: keep P1 singleton, instantiate g_Player2 as second instance.
   - All FUN_*'s already in source, just need P2 awareness.
4. **EnemyManager / BulletManager**:
   - Hit detection takes both player positions.
   - Aim-at-player picks nearest player.
5. **ItemManager**: route by nearest player.
6. **GameManager**: separate lives/bombs counters per player.
7. **HUD**: render P2 stats alongside P1.
8. **Pause sync, spell card sync, death sync**: all naturally fall
   out of deterministic lockstep — both peers run the same sim, pause
   from either peer's ESC press is broadcast, etc.

**Exit criteria**: 2-instance test on loopback, both peers see
identical enemy patterns, both players visible, both can shoot, both
can die independently, ESC from either pauses both, score works.

---

## 3. Tooling — what's used per phase

| Tool | Purpose | Phase |
|---|---|---|
| IDA Pro 9.2 + Hex-Rays | Authoritative pseudocode + struct layout | β, γ, δ |
| `python tools/ida_helper.py call ...` | MCP client to IDA at 127.0.0.1:13337 | β, γ |
| `python tools/batch_analyze*.py` | Pre-analyze N functions → worksheet | β, γ |
| `python scripts/build.py` | Ninja build (in th08-decomp) | all |
| `reccmp-reccmp --target th08 --json ...` | Byte-match measurement | all |
| `codex exec ... -m gpt-5.4` | Worker for bulk function bodies | β, γ, ε |
| `git diff` + manual review | Surgical revert of regressions (batch 11 lesson) | all |
| `config/binary_patches.json` | Byte-patch .obj for unfixable codegen | α |
| Vanilla replay files | Behavioral diff testing | δ |

User now permits IDA to be opened. To start IDA in the background:
```
start "" "D:\Software\IDA Professional 9.2\ida.exe" "D:\Project\THGlobal\th08\th08.exe.i64"
```
The MCP server inside the .i64 starts on port 13337 once IDA is loaded.

---

## 4. Risks + mitigation

| Risk | Probability | Impact | Mitigation |
|---|---|---|---|
| **codex runs out of capacity** mid-batch (provider 502s) | high | per-batch | gpt-5.4 over 5.5; persist worksheets to git so next session resumes |
| **Phase β bogs at <50% rate** (functions too complex) | medium | weeks slip | switch to manual implementation for 5-10 critical fns; codex handles the long tail |
| **Compiler version mismatch** introduces irreducible byte diffs | medium | lower byte-match ceiling | accept 85-90% as pragmatic; binary patcher for forced matches |
| **Phase ε ABI bug** crashes the multiplayer fork | medium | 1-2 weeks debugging | start ε with 1P-only build first, verify still runs, then add 2P incrementally |
| **IDA license expires / crashes** | low | day | save .i64 frequently; backup in git LFS if user agrees |

---

## 5. Decision points

You (dwgx) get to make these calls:

| Decision | When | Default if no input |
|---|---|---|
| Open IDA + start MCP server | now (you've already permitted) | Claude tries to launch via PowerShell start |
| Phase α (binary patcher batch) yes/no | end of week 1 | yes — proven precedent |
| Spend codex tokens on speculative refactors | per batch | no — only verified-win edits |
| Accept partial-byte-match as "done" for non-critical | per module | yes — pragmatic |
| Open-source the multinet fork | end of phase ε | TBD — your call |

---

## 6. Immediate next actions (this week)

If you OK this plan:

1. **Today**: I open IDA (or you do), confirm MCP at :13337 responds.
2. **Day 1**: Run `python tools/batch_analyze.py` against the 12
   Player <50% functions → worksheets/wsN_player.md.
3. **Day 1-2**: Codex batch 13 dispatched on Player worksheet. Goal:
   close 6-9 Player functions to ≥95%.
4. **Day 2**: Same for GameManager 13 low-match functions → worksheet
   → codex batch 14.
5. **Day 3-5**: EnemyManager + BulletManager + Item / ItemManager.
6. **Week 2**: Background, ResultScreen, EclManager.
7. **Week 3-4**: Phase γ (stage script VM polish + replay test).
8. **Month 2**: Phase δ (gameplay bug bash).
9. **Month 3**: Phase ε (multiplayer fork).

---

## 7. Status tracking

This plan is checkpointed in:
- `TH08-Platform/docs/decomp_master_plan.md` (this file)
- `th08-decomp/diff_report.json` (live byte-match snapshot)
- `memory/02_current_status.md` (will be updated each session end)

Each codex batch should update a row in
`TH08-Platform/docs/decomp_progress.csv` (to be created):
```csv
batch,date,target_module,fns_attempted,fns_won,fns_partial,perfect_after
10,2026-04-26,various-medium-high,16,1,14,425
11,2026-04-26,various-medium-mid,30,2,2,427
12,2026-04-26,linkage-3groups,15,0,0,427
```

So we have a single CSV view of progress over time.

---

*This plan is the contract. If a phase blows up its estimate by >50%
the plan gets rewritten with the user, not silently extended.*
