# Decomp Batch 11 — Medium 70-95% TH08 functions

Generated 2026-04-26 from `th08-decomp/diff_report.json`. Run AFTER batch 10
(`a685886`+ era) to get fresh baseline first.

## Worksheet (30 targets)

All in `th08::` namespace, currently 70-95% byte-matched. Sorted by current %.
Goal: bring as many to 100% (matching=1.0) as possible. Same iteration loop as
batch 10: read `src/<file>.cpp`, compare with `diff_report.json`'s `diff` array,
apply targeted source edits, build, reccmp, verify.

| addr     | name                                            | current % |
|----------|-------------------------------------------------|-----------|
| 0x402000 | th08::AsciiManager::AsciiManager                | 94.59     |
| 0x441e10 | th08::AnmManager::`scalar deleting destructor'  | 94.44     |
| 0x402190 | th08::RetryMenu::RetryMenu                      | 94.44     |
| 0x402150 | th08::PauseMenu::PauseMenu                      | 94.44     |
| 0x440010 | th08::ItemManager::ItemManager                  | 94.12     |
| 0x43c641 | th08::GameManager::AddLives                     | 94.12     |
| 0x439883 | th08::GameManager::AddToBombCount               | 94.12     |
| 0x442200 | th08::GameWindow::InitD3DInterface              | 93.75     |
| 0x43ed80 | th08::Rng::GetRandomF32Signed                   | 92.86     |
| 0x42eb90 | th08::EnemyManager::OnDrawLowPrio               | 92.86     |
| 0x4021d0 | th08::AsciiManagerPopup::AsciiManagerPopup      | 92.86     |
| 0x447421 | th08::Supervisor::TickTimer                     | 92.50     |
| 0x442ef0 | th08::GameWindow::FormatCapability              | 92.31     |
| 0x449254 | th08::MusicRoom::RegisterChain                  | 92.19     |
| 0x45b020 | th08::ScreenEffect::Clear                       | 91.60     |
| 0x4663b0 | th08::AnmManager::DrawVmTextFmt                 | 91.43     |
| 0x429d40 | th08::Ending::DeletedCallback                   | 91.43     |
| 0x449331 | th08::MusicRoom::MusicRoom                      | 91.18     |
| 0x402130 | th08::AsciiManagerString::AsciiManagerString    | 90.91     |
| 0x40b580 | th08::VertexDiffuseXyzrhw::VertexDiffuseXyzrhw  | 90.00     |
| 0x45b760 | th08::ScreenEffect::CalcFadeOut                 | 89.80     |
| 0x445400 | th08::DummyMidiTimer::OnTimerElapsed            | 88.89     |
| 0x406660 | th08::ZunTimer::Tick                            | 88.89     |
| 0x4068e0 | th08::AnmVm::Initialize                         | 87.72     |
| 0x454c59 | th08::ResultScreen::LinkScoreEx                 | 86.67     |
| 0x45a5a0 | th08::ScoreDat::FreeAllScores                   | 86.36     |
| 0x454c87 | th08::ResultScreen::FreeScore                   | 85.71     |
| 0x4398ff | th08::AsciiManager::SetIsGuiMode                | 84.21     |
| 0x440050 | th08::Item::Item                                | 83.02     |
| 0x44c390 | th08::Player::OnUpdate                          | 81.78     |

## Dispatch template (copy to bash)

```bash
filename=$(openssl rand -hex 4)
LOG=/tmp/codex-batch11-${filename}.log
STD=/tmp/codex-batch11-${filename}.stdout
codex exec \
    --skip-git-repo-check \
    --dangerously-bypass-approvals-and-sandbox \
    -C "D:/Project/THGlobal/th08-decomp" \
    --config model="gpt-5.4" \
    --config model_reasoning_effort="high" \
    "$(cat TH08-Platform/docs/worksheets/decomp_batch11_medium.md | sed -n '/^## Prompt for codex/,$p')" \
    > "$STD" 2>"$LOG" &
echo "pid=$!  log=$LOG  stdout=$STD"
```

## Prompt for codex

You're extending byte-match coverage for th08-decomp. The repo at the
current cwd is the working copy. Process the 30 functions from the
"Worksheet" table in this file (TH08-Platform/docs/worksheets/decomp_batch11_medium.md).

# Workflow

1. Refresh diff_report:
   ```
   python scripts/build.py
   cd build && reccmp-reccmp --target th08 --json ../diff_report.json
   cd ..
   ```

2. For each function in the worksheet:
   a. Find in `src/*.cpp`.
   b. Read the diff_report.json entry by name.
   c. Compare orig vs recomp asm in the diff hunks.
   d. Edit source to close gaps.
   e. Common patterns: constant tweaks, helper inlining via
      `__forceinline`, local variable order via `#pragma var_order(...)`,
      branch flip (if/else), magic-number alignment.

3. Build + reccmp once after editing all 30. Then ONE iteration on
   functions that remain partial (matching < 1.0).

4. Emit `FINAL_STATS:` JSON same as batch 10.

# Constraints

- DO NOT touch scripts/configure.py codegen flags (Gui stays /Os, others
  /Od /RTCu).
- DO NOT modify tools/ or anywhere outside this repo.
- Skip linkage-only mismatches (CRT / D3DX call name differences); those
  are batch 12's problem.
- Max 2 build+reccmp cycles. Stop and report if a 3rd is needed.
- Token budget: ~200k codex tokens.
