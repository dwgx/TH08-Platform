# Decomp Batch 12 — Near-miss linkage mismatches (the 13 ≥99% functions)

Generated 2026-04-26. The 13 near-miss functions in `diff_report.json` all
share the same root cause: **they call the right C/D3D/MFC routine but the
binary import name differs from TH08's**, so reccmp asm-diff sees the call
as a mismatch even though runtime behavior is identical.

Without a config-level fix, these 13 are uncloseable from source edits alone.
This worksheet captures the exact gap per group and proposes the build-system
fix.

## The 3 groups

### Group A — `time` vs `_time32` (2-3 callers)

| Function | Where time is called |
|---|---|
| 0x4464c8 Supervisor::StartupThread | `src/Supervisor.cpp:649` `time(&currentTime)` |
| (also) | `src/ResultScreen.cpp:116` `time(&currentTime)` |

Modern MSVC defaults `time_t = __time64_t` and `time = _time64`. TH08 (built
with VS2002 + DX8 SDK) used the only-then-existing `_time32`. To force our
build to use `_time32`, add `/D_USE_32BIT_TIME_T` to `cl_common_flags` in
`scripts/configure.py:51`.

**Risk**: low — `time_t` is only used in those 2 spots, both already use the
returned value as a 32-bit integer for date formatting. Re-checking other uses
of `time_t` is the codex's job before changing the flag.

### Group B — `vector ctor iter` vs `eh_vector ctor iter` (4-5 callers)

| Function |
|---|
| 0x44970f MusicRoom::AddedCallback |
| 0x449487 MusicRoom::OnDraw |
| 0x448b16 MusicRoom::ProcessInput |
| (and probably) all callers that construct an array of types with destructors |

`/EHsc` (synchronous + `extern "C"` no-throw) generates the non-EH variant
when the destructor is provably non-throwing. TH08 was probably compiled
with `/EHs` or `/EHa` which forced the EH variant.

**Hypothesis**: try `/EHa` instead of `/EHsc`. Risk: medium — could regress
many other functions if they currently match because of `/EHsc` behavior.
Run reccmp before/after and compare overall perfect count.

Alternative if `/EHa` regresses: leave these 3-5 as permanent partials, accept
99.5%+ as good enough for decomp purposes (the binaries are functionally
identical).

### Group C — D3DX import name decoration (5 callers)

| Function |
|---|
| 0x43f9c8 TextHelper::RenderTextToTextureBold (calls D3DXLoadSurfaceFromSurface) |
| 0x43fce9 TextHelper::RenderTextToTexture (same) |
| 0x463210 AnmManager::Draw2D (calls D3DX...) |
| 0x466bb0 AnmManager::CopySurfaceToBackbuffer (calls D3DX...) |
| 0x441e70 GameWindow::Render (calls D3DX...) |

Orig calls `D3DXLoadSurfaceFromSurface` (undecorated). Recomp calls
`_D3DXLoadSurfaceFromSurface@32` (stdcall + 8-arg decoration).

Root cause: the original DirectX 8.0 SDK's d3dx8.lib exported these symbols
WITHOUT the stdcall decoration; modern d3dx8.lib (from DX 9.0c SDK or merged
imports) exports them WITH `@N` decoration. Same DLL function but different
symbol naming in the import library.

**Fix options**:
1. **Use the original DX 8.0 d3dx8.lib** — the build's `create_devenv.py`
   downloads dependencies; check what version it pulls. If newer, swap for
   DX 8.0 SDK's lib. (recommended)
2. **Custom .def file** that re-exports the decorated names as undecorated
   aliases. Complicated.
3. **Accept as permanent partials** at ≥99% — simplest.

## Workflow for codex (after batch 10/11 finish)

```
1. Backup current diff_report.json baseline (cp diff_report.json
   diff_report.before-batch12.json).
2. Try Group A: add /D_USE_32BIT_TIME_T to cl_common_flags.
   Build + reccmp. Diff vs backup. If 2-3 of the 13 went green and
   nothing else regressed, KEEP. If anything regressed by >2 points,
   REVERT.
3. Try Group B: change /EHsc to /EHa.
   Build + reccmp. Diff vs current. If MusicRoom group went green AND
   net perfect count is +N (N>=2), KEEP. If net is negative, REVERT.
4. Group C: investigate scripts/create_devenv.py and 3rdparty/dx for
   d3dx8.lib version. If swappable to DX 8.0 SDK lib without breaking
   build, do it. If not feasible, document as permanent partials.
5. Emit FINAL_STATS as before, with extra fields:
   {
     "group_a_kept": bool,
     "group_b_kept": bool,
     "group_c_action": "kept|reverted|skipped",
     "perfect_added_total": N,
     "regressions": M,
     "permanent_partials": [...]   // names accepted as <100%
   }
```

## Constraints

- This is the only batch that's allowed to touch scripts/configure.py.
- Build flag changes can have wide blast radius — VERIFY net win via
  reccmp after each change. Revert if net negative.
- Max 3 build+reccmp cycles (one per group).
- Token budget: ~150k codex tokens.
