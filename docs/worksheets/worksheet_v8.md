# Worksheet v8: ResultScreen + Supervisor + ScoreDat + AnmManager + ScreenEffect

- ResultScreen: 10
- Supervisor:   8
- ScoreDat:     6
- AnmManager:   4
- ScreenEffect: 4

**Total candidates**: 32

All on /Od /RTCu codegen (see scripts/configure.py for per-source flag overrides).

Sorted: highest current match% first within each module (near-misses cheaper to push to 100%).

---

# ResultScreen

## th08::ResultScreen::LinkScoreEx @ 0x454c59 (size=0x2e, current match=86.67%)

- conv: `__thiscall` | ret: `void` | args: `ResultScreen*, void*, i32, i32`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::ResultScreen::FreeScore @ 0x454c87 (size=0x2b, current match=85.71%)

- conv: `__thiscall` | ret: `void` | args: `ResultScreen*, i32, i32`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::ResultScreen::MoveCursor @ 0x45a0f4 (size=0xff, current match=80.00%)

- conv: `__fastcall` | ret: `u32` | args: `ResultScreen*, i32`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::ResultScreen::MoveShotTypeCursor @ 0x45a1f3 (size=0xff, current match=80.00%)

- conv: `__fastcall` | ret: `ZunResult` | args: `ResultScreen*, i32`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::ResultScreen::MoveCursorHorizontally @ 0x45a2f2 (size=0x10b, current match=80.00%)

- conv: `__fastcall` | ret: `i32` | args: `ResultScreen*, i32`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::ResultScreen::FormatDate @ 0x456938 (size=0x37, current match=76.19%)

- conv: `__fastcall` | ret: `void` | args: `char*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::ResultScreen::GetStageName @ 0x453cd1 (size=0x29, current match=60.00%)

- conv: `__fastcall` | ret: `u8` | args: `u32`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::ResultScreen::HandleHighScoreScreen @ 0x455925 (size=0x10e, current match=46.15%)

- conv: `__thiscall` | ret: `u32` | args: `ResultScreen*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::ResultScreen::CheckConfirmButton @ 0x4577e2 (size=0xc8, current match=36.36%)

- conv: `__thiscall` | ret: `ZunResult` | args: `ResultScreen*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::ResultScreen::DeletedCallback @ 0x459fd2 (size=0xe1, current match=36.36%)

- conv: `__fastcall` | ret: `u8` | args: `u32`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

# Supervisor

## th08::Supervisor::UpdatePlayTime @ 0x4482a1 (size=0x177, current match=98.13%)

- conv: `__thiscall` | ret: `void` | args: `Supervisor*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::Supervisor::UpdateGameTime @ 0x448418 (size=0x175, current match=98.11%)

- conv: `__thiscall` | ret: `void` | args: `Supervisor*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::Supervisor::LoadDat @ 0x4461a0 (size=0x92, current match=97.67%)

- conv: `__stdcall` | ret: `ZunResult` | args: ``

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::Supervisor::DeleteCriticalSections @ 0x448a2f (size=0x36, current match=95.24%)

- conv: `__thiscall` | ret: `void` | args: `Supervisor*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::Supervisor::TickTimer @ 0x447421 (size=0x6e, current match=92.50%)

- conv: `__thiscall` | ret: `void` | args: `Supervisor*, i32*, f32*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::Supervisor::StopAudio @ 0x448081 (size=0x77, current match=72.73%)

- conv: `__thiscall` | ret: `u32` | args: `Supervisor*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::Supervisor::IsSlowModeEnabled @ 0x4481af (size=0x30, current match=66.67%)

- conv: `__thiscall` | ret: `u8` | args: `Supervisor*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::Supervisor::DrawFpsCounter @ 0x445bc0 (size=0x14, current match=57.14%)

- conv: `__fastcall` | ret: `ChainCallbackResult` | args: `Supervisor*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

# ScoreDat

## th08::ScoreDat::FreeAllScores @ 0x45a5a0 (size=0x3b, current match=86.36%)

- conv: `__fastcall` | ret: `u8` | args: `ScoreListNode*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::ScoreDat::LinkScore @ 0x45a500 (size=0x9f, current match=28.57%)

- conv: `__fastcall` | ret: `i32` | args: `ScoreListNode*, Hscr*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::ScoreDat::GetHighScore @ 0x45a950 (size=0x16a, current match=25.00%)

- conv: `__fastcall` | ret: `u32` | args: `ScoreDat*, u32, u32, u32, uchar*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::ScoreDat::ParsePLST @ 0x45af30 (size=0x81, current match=23.53%)

- conv: `__fastcall` | ret: `ZunResult` | args: `ScoreDat*, Plst*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::ScoreDat::ParseLSNM @ 0x45ab70 (size=0x88, current match=23.53%)

- conv: `__fastcall` | ret: `i32` | args: `ScoreDat*, Lsnm*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::ScoreDat::ParseFLSP @ 0x45ac00 (size=0x88, current match=23.53%)

- conv: `__fastcall` | ret: `i32` | args: `ScoreDat*, Flsp*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

# AnmManager

## th08::AnmManager::CreateEmptyTexture @ 0x4657c0 (size=0x38, current match=95.65%)

- conv: `__thiscall` | ret: `u32` | args: `AnmManager*, IDirect3DTexture8**, i32, i32, i32`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::AnmManager::DrawVmTextFmt @ 0x4663b0 (size=0xee, current match=88.24%)

- conv: `__cdecl` | ret: `u8` | args: `AnmManager*, u32, u32, u32, u32`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::AnmManager::LoadExternalTextureData @ 0x465ac0 (size=0xdb, current match=30.38%)

- conv: `__thiscall` | ret: `ZunResult` | args: `AnmManager*, AnmFileDesc*, i32, i32*, i32*, AnmRawEntry*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::AnmManager::`scalar_deleting_destructor' @ 0x441e10 (size=0x2e, current match=0.00%)

- conv: `__thiscall` | ret: `AnmManager*` | args: `AnmManager*, u32`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

# ScreenEffect

## th08::ScreenEffect::CalcFadeIn @ 0x45b160 (size=0x7b, current match=95.00%)

- conv: `__fastcall` | ret: `bool` | args: `ScreenEffect*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::ScreenEffect::Clear @ 0x45b020 (size=0xbe, current match=66.67%)

- conv: `__fastcall` | ret: `u8` | args: `unsigned long`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::ScreenEffect::CalcFadeOut @ 0x45b760 (size=0x9b, current match=57.14%)

- conv: `__fastcall` | ret: `u32` | args: `ScreenEffect*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

## th08::ScreenEffect::DrawFullFade @ 0x45bb50 (size=0x91, current match=30.77%)

- conv: `__fastcall` | ret: `u32` | args: `ScreenEffect*`

_(get_bytes failed: 'list' object has no attribute 'get')_

---

