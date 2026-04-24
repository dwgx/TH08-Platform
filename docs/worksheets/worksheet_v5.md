# Worksheet v5: Near-misses + Untouched modules

- **A. Near-misses (>=92%)**: 10
- **B. Untouched-module small functions**: 20

**Total**: 30

Target: close the gap on near-matches + seed brand-new modules.
---

# A. Near-misses (closest to 100%)

## th08::GameWindow::ResolveIt @ 0x443840 (size=0x142) (currently 99.0%)

- Sig: `ZunBool GameWindow::ResolveIt(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::SoundPlayer::StopBGM @ 0x45d2d0 (size=0x11f) (currently 98.7%)

- Sig: `void SoundPlayer::StopBGM(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::GameWindow::Present @ 0x442060 (size=0x127) (currently 98.7%)

- Sig: `void GameWindow::Present(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::AnmManager::DrawVmTextFmt @ 0x4663b0 (size=0xee) (currently 98.6%)

- Sig: `u8 AnmManager::DrawVmTextFmt(...)` [__cdecl]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::Supervisor::RegisterChain @ 0x445ea7 (size=0xfc) (currently 98.5%)

- Sig: `ZunResult Supervisor::RegisterChain(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::CSoundManager::Initialize @ 0x471730 (size=0x8e) (currently 98.2%)

- Sig: `long CSoundManager::Initialize(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::Supervisor::LoadDat @ 0x4461a0 (size=0x92) (currently 97.7%)

- Sig: `ZunResult Supervisor::LoadDat(...)` [__stdcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::Player::DeletedCallback @ 0x44dc60 (size=0xa4) (currently 96.6%)

- Sig: `ZunResult Player::DeletedCallback(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## D3DXVECTOR3::operator+ @ 0x409080 (size=0x46) (currently 96.4%)

- Sig: `_D3DVECTOR* ::operator+(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## D3DXVECTOR3::operator- @ 0x4090d0 (size=0x46) (currently 96.4%)

- Sig: `_D3DVECTOR* ::operator-(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

# B. Untouched modules (fresh territory)

## th08::Gui::AddedCallback @ 0x437a2f (size=0x11) (currently 66.7%)

- Sig: `u8 Gui::AddedCallback(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::ResultScreen::GetCharacterName @ 0x453cfa (size=0x13) (currently 66.7%)

- Sig: `u8 ResultScreen::GetCharacterName(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::ResultScreen::~ResultScreen @ 0x45a0dc (size=0x18)

- Sig: `void ResultScreen::~ResultScreen(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::ScreenEffect::ScreenEffect @ 0x45b000 (size=0x19)

- Sig: `u8 ScreenEffect::ScreenEffect(...)` [unknown]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::Gui::CutChain @ 0x437d64 (size=0x23) (currently 66.7%)

- Sig: `u8 Gui::CutChain(...)` [unknown]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::ResultScreen::GetStageName @ 0x453cd1 (size=0x29) (currently 66.7%)

- Sig: `u8 ResultScreen::GetStageName(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::ResultScreen::`scalar_deleting_destructor' @ 0x45a0b3 (size=0x29)

- Sig: `void* ResultScreen::`scalar_deleting_destructor'(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::ResultScreen::FreeScore @ 0x454c87 (size=0x2b) (currently 72.7%)

- Sig: `void ResultScreen::FreeScore(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::ResultScreen::LinkScoreEx @ 0x454c59 (size=0x2e) (currently 72.7%)

- Sig: `void ResultScreen::LinkScoreEx(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::Background::CutChain @ 0x409ca0 (size=0x32) (currently 66.7%)

- Sig: `void Background::CutChain(...)` [__stdcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::ResultScreen::FormatDate @ 0x456938 (size=0x37) (currently 40.0%)

- Sig: `void ResultScreen::FormatDate(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::Gui::FreeMsgFile @ 0x4397d5 (size=0x3b) (currently 80.0%)

- Sig: `void Gui::FreeMsgFile(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::Gui::MsgWait @ 0x43587e (size=0x3d)

- Sig: `bool Gui::MsgWait(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::ResultScreen::SetState @ 0x4550b7 (size=0x45) (currently 72.7%)

- Sig: `void ResultScreen::SetState(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::ScreenEffect::DrawArcadeFade @ 0x45bc40 (size=0x45) (currently 30.8%)

- Sig: `u32 ScreenEffect::DrawArcadeFade(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::Ending::ReadEndFileParameter @ 0x428890 (size=0x73) (currently 30.8%)

- Sig: `i32 Ending::ReadEndFileParameter(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::Background::DeletedCallback @ 0x409c20 (size=0x7a) (currently 44.4%)

- Sig: `u32 Background::DeletedCallback(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::ScreenEffect::CalcFadeIn @ 0x45b160 (size=0x7b) (currently 53.3%)

- Sig: `bool ScreenEffect::CalcFadeIn(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::ScreenEffect::SetViewport @ 0x45b0e0 (size=0x7c) (currently 72.7%)

- Sig: `void ScreenEffect::SetViewport(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::Ending::OnDraw @ 0x4298f0 (size=0x83) (currently 30.8%)

- Sig: `ChainCallbackResult Ending::OnDraw(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

