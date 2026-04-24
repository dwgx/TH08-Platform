# Worksheet v4

## Critical Systemic Fix

Before implementing Category A functions, do this fix:

**Move `IsTampered()` from inline in GameManager.hpp to out-of-line stub in GameManager.cpp.**

Reason: Current `void IsTampered() { return FALSE; }` inline gets DCE'd, causing the `if (IsTampered()) CRASH_GAME();` branches in AddLives/AddToBombCount/etc. to be eliminated entirely. To byte-match the original (which has the branch), IsTampered needs to be a real callable function.

- In src/GameManager.hpp, change:
    ```cpp
    ZunBool IsTampered() { return FALSE; }
    ```
  to:
    ```cpp
    ZunBool IsTampered();
    ```
- In src/GameManager.cpp, add (alongside UpdateAntiTamper):
    ```cpp
    ZunBool GameManager::IsTampered()
    {
        return FALSE;
    }
    ```

After this fix, try rebuilding and see if AddLives/AddToBombCount/etc jump up naturally ¡ª they may even reach 100% if codex already implemented their bodies correctly.

---

# Categories

- **A. Near-misses (>=80% match)**: 6
- **B. AsciiManager small unimpl**: 6
- **C. AnmManager small unimpl**: 6
- **D. GameWindow small unimpl**: 2
- **E. ItemManager small unimpl**: 1
- **F. ScoreDat small unimpl**: 4

**Total (excluding the IsTampered fix)**: 25

All modules already on /Od /RTCu.

---

# A. Near-misses

## th08::AnmManager::DrawVmTextFmt @ 0x4663b0 (size=0xee) (currently 98.6%)

- Signature: `u8 AnmManager::DrawVmTextFmt(...)` [__cdecl]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::Supervisor::RegisterChain @ 0x445ea7 (size=0xfc) (currently 98.5%)

- Signature: `ZunResult Supervisor::RegisterChain(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::CSoundManager::Initialize @ 0x471730 (size=0x8e) (currently 98.2%)

- Signature: `long CSoundManager::Initialize(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::Supervisor::LoadDat @ 0x4461a0 (size=0x92) (currently 97.7%)

- Signature: `ZunResult Supervisor::LoadDat(...)` [__stdcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::Player::DeletedCallback @ 0x44dc60 (size=0xa4) (currently 96.6%)

- Signature: `ZunResult Player::DeletedCallback(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## D3DXVECTOR3::operator+ @ 0x409080 (size=0x46) (currently 96.4%)

- Signature: `_D3DVECTOR* ::operator+(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

# B. AsciiManager small

## th08::AsciiManager::GetInterrupt @ 0x407140 (size=0x14)

- Signature: `u32 AsciiManager::GetInterrupt(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::AsciiManager::SetIsGuiMode @ 0x4398ff (size=0x17)

- Signature: `void AsciiManager::SetIsGuiMode(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::AsciiManager::SetScale @ 0x42f2f0 (size=0x25)

- Signature: `void AsciiManager::SetScale(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::AsciiManager::SetBossMarkerPosition @ 0x422be0 (size=0x33)

- Signature: `void AsciiManager::SetBossMarkerPosition(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::AsciiManager::OnDrawLowPrio @ 0x4023d0 (size=0x5f) (currently 92.6%)

- Signature: `ChainCallbackResult AsciiManager::OnDrawLowPrio(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::AsciiManager::SetInterrupt @ 0x4070b0 (size=0x65)

- Signature: `void AsciiManager::SetInterrupt(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

# C. AnmManager small

## th08::AnmManager::ClearZWriteSetting @ 0x40ba30 (size=0x15)

- Signature: `void AnmManager::ClearZWriteSetting(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::AnmManager::ClearSprite @ 0x40ba50 (size=0x18)

- Signature: `u8 AnmManager::ClearSprite(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::AnmManager::SetCameraMode @ 0x40ba90 (size=0x19)

- Signature: `u8 AnmManager::SetCameraMode(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::AnmManager::`scalar_deleting_destructor' @ 0x441e10 (size=0x2e)

- Signature: `AnmManager* AnmManager::`scalar_deleting_destructor'(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::AnmManager::CreateEmptyTexture @ 0x4657c0 (size=0x38) (currently 95.7%)

- Signature: `u32 AnmManager::CreateEmptyTexture(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::AnmManager::CreateTextureFromFile @ 0x465570 (size=0x62)

- Signature: `ZunResult AnmManager::CreateTextureFromFile(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

# D. GameWindow small

## th08::GameWindow::InitD3DInterface @ 0x442200 (size=0x38) (currently 93.8%)

- Signature: `ZunBool GameWindow::InitD3DInterface(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::GameWindow::FormatCapability @ 0x442ef0 (size=0x6b) (currently 92.3%)

- Signature: `char* GameWindow::FormatCapability(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

# E. ItemManager small

## th08::ItemManager::ItemManager @ 0x440010 (size=0x34) (currently 94.1%)

- Signature: `ItemManager* ItemManager::ItemManager(...)` [__thiscall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

# F. ScoreDat small

## th08::ScoreDat::ReleaseScore @ 0x45afc0 (size=0x35) (currently 66.7%)

- Signature: `void ScoreDat::ReleaseScore(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::ScoreDat::FreeAllScores @ 0x45a5a0 (size=0x3b) (currently 36.4%)

- Signature: `u8 ScoreDat::FreeAllScores(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::ScoreDat::ParsePLST @ 0x45af30 (size=0x81) (currently 23.5%)

- Signature: `ZunResult ScoreDat::ParsePLST(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

## th08::ScoreDat::ParseLSNM @ 0x45ab70 (size=0x88) (currently 23.5%)

- Signature: `i32 ScoreDat::ParseLSNM(...)` [__fastcall]
- Hex-Rays:

```c
(no hexrays)
```

- Bytes: ``

---

