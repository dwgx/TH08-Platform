# Worksheet v6

- A. Near-misses (68%+): 12
- B. Player class small: 0
- C. Ending fresh: 3
- D. ScoreDat fresh: 2

**Total**: 17

---

# A. Near-misses (68%-99%)

## th08::GameWindow::ResolveIt @ 0x443840 (size=0x142) (currently 99.0%)

- Sig: `ZunBool GameWindow::ResolveIt(...)` [__fastcall]
- Hex-Rays:

```c
int __fastcall sub_443840(const CHAR *a1, int a2, int cchWideChar)
{
  _BYTE v6[324]; // [esp+10h] [ebp-158h] BYREF
  LPWSTR lpWideCharStr; // [esp+154h] [ebp-14h]
  int v8; // [esp+158h] [ebp-10h] BYREF
  LPVOID ppv; // [esp+15Ch] [ebp-Ch] BYREF
  int v10; // [esp+160h] [ebp-8h]
  HRESULT v11; // [esp+164h] [ebp-4h]

  if ( !a2 ) /*0x44385c*/
    return 0; /*0x44385e*/
  v10 = 0; /*0x443865*/
  CoInitialize(0); /*0x44386e*/
  v11 = CoCreateInstance(&rclsid, 0, 1u, &riid, &ppv); /*0x44388c*/
  if ( v11 >= 0 ) /*0x443893*/
  {
    v11 = (**(int (__stdcall ***)(LPVOID, void *, int *))ppv)(ppv, &unk_4BD6F0, &v8); /*0x4438ad*/
    if ( v11 >= 0 ) /*0x4438b4*/
    {
      lpWideCharStr = (LPWSTR)operator new(2 * cchWideChar); /*0x4438d4*/
      if ( v11 >= 0 ) /*0x4438db*/
      {
        MultiByteToWideChar(0, 0, a1, -1, lpWideCharStr, cchWideChar); /*0x4438f2*/
        v11 = (*(int (__stdcall **)(int, LPWSTR, _DWORD))(*(_DWORD *)v8 + 20))(v8, lpWideCharStr, 0); /*0x44390a*/
        if ( v11 >= 0 ) /*0x443911*/
        {
          v11 = (*(int (__stdcall **)(LPVOID, int, int, _BYTE *, _DWORD))(*(_DWORD *)ppv + 12))( /*0x443933*/
                  ppv,
                  a2,
                  cchWideChar,
                  v6,
                  0);
          if ( v11 >= 0 ) /*0x44393a*/
            v10 = 1; /*0x44393c*/
        }
      }
      j__free(lpWideCharStr); /*0x443953*/
      (*(void (__stdcall **)(int))(*(_DWORD *)v8 + 8))(v8); /*0x443964*/
    }
    (*(void (__stdcall *
... [truncated]
```

- Callees: CoCreateInstance, CoUninitialize, CoInitialize, j__free, MultiByteToWideChar, ??2@YAPAXI@Z

- Bytes: `0x55 0x8b 0xec 0x81 0xec 0x68 0x1 0x0 0x0 0x89 0x95 0x98 0xfe 0xff 0xff 0x89 0x8d 0x9c 0xfe 0xff 0xff 0x83 0xbd 0x98 0xfe 0xff 0xff 0x0 0x75 0x7 0x33 0xc0 0xe9 0x17 0x1 0x0 0x0 0xc7 0x45 0xf8 0x0 0x0 ...`

---

## th08::SoundPlayer::StopBGM @ 0x45d2d0 (size=0x11f) (currently 98.7%)

- Sig: `void SoundPlayer::StopBGM(...)` [__thiscall]
- Hex-Rays:

```c
int __thiscall sub_45D2D0(int this)
{
  int result; // eax

  result = this; /*0x45d2d9*/
  if ( *(_DWORD *)(this + 21000) ) /*0x45d2dc*/
  {
    sub_421DD0("Streming BGM stop\r\n"); /*0x45d2ee*/
    result = sub_4728E0(*(_DWORD *)(this + 21000)); /*0x45d2ff*/
    if ( *(_DWORD *)(this + 1560) ) /*0x45d307*/
    {
      PostThreadMessageA(*(_DWORD *)(this + 1556), 0x12u, 0, 0); /*0x45d324*/
      sub_421DD0("stop m_dwNotifyThreadID\r\n"); /*0x45d32f*/
      while ( WaitForSingleObject(*(HANDLE *)(this + 1560), 0x100u) ) /*0x45d34e*/
        PostThreadMessageA(*(_DWORD *)(this + 1556), 0x12u, 0, 0); /*0x45d360*/
      sub_421DD0("stop comp\r\n"); /*0x45d36d*/
      CloseHandle(*(HANDLE *)(this + 1560)); /*0x45d37f*/
      result = CloseHandle(*(HANDLE *)(this + 21004)); /*0x45d38f*/
      *(_DWORD *)(this + 1560) = 0; /*0x45d398*/
    }
    if ( *(_DWORD *)(this + 21000) ) /*0x45d3a5*/
    {
      (***(void (__thiscall ****)(_DWORD, int))(this + 21000))(*(_DWORD *)(this + 21000), 1); /*0x45d3d0*/
      *(_DWORD *)(this + 21000) = 0; /*0x45d3e1*/
      return this; /*0x45d3de*/
    }
  }
  return result; /*0x45d3eb*/
}
```

- Callees: PostThreadMessageA, WaitForSingleObject, sub_421DD0¡ú`th08::utils::DebugPrint`, sub_4728E0¡ú`th08::CSound::Stop`, CloseHandle

- Bytes: `0x55 0x8b 0xec 0x83 0xec 0x10 0x89 0x4d 0xf4 0x8b 0x45 0xf4 0x83 0xb8 0x8 0x52 0x0 0x0 0x0 0xf 0x84 0x2 0x1 0x0 0x0 0x68 0x98 0x82 0x4b 0x0 0xe8 0xdd 0x4a 0xfc 0xff 0x83 0xc4 0x4 0x8b 0x4d 0xf4 0x8b 0...`

---

## th08::GameWindow::Present @ 0x442060 (size=0x127) (currently 98.7%)

- Sig: `void GameWindow::Present(...)` [__fastcall]
- Hex-Rays:

```c
int sub_442060()
{
  int result; // eax
  char Buffer[260]; // [esp+0h] [ebp-108h] BYREF
  int i; // [esp+104h] [ebp-4h]

  if ( (*(int (__stdcall **)(int, _DWORD, _DWORD, _DWORD, _DWORD))(*(_DWORD *)dword_17CE760 + 60))( /*0x442084*/
         dword_17CE760,
         0,
         0,
         0,
         0) < 0 )
  {
    sub_443AF0(dword_18BDC90); /*0x44208c*/
    (*(void (__stdcall **)(int, void *))(*(_DWORD *)dword_17CE760 + 56))(dword_17CE760, &unk_17CE838); /*0x4420a4*/
    sub_442F60(); /*0x4420a7*/
    dword_17CE8CC = 2; /*0x4420ac*/
  }
  sub_443B60(dword_18BDC90); /*0x4420bc*/
  result = word_164D528 & 0x800; /*0x4420c8*/
  if ( (word_164D528 & 0x800) != 0 && (word_164D528 & 0x800) != (word_164D530 & 0x800) ) /*0x4420f1*/
  {
    result = _mkdir("snapshot"); /*0x4420f8*/
    for ( i = 0; i < 1000; ++i ) /*0x442100*/
    {
      sprintf(Buffer, "snapshot/th%.3d.bmp", i); /*0x44212b*/
      result = sub_43E880(Buffer); /*0x442139*/
      if ( !result ) /*0x442140*/
        break; /*0x442140*/
      result = i + 1; /*0x44210c*/
    }
    if ( i < 1000 ) /*0x44214d*/
      result = sub_44748F(&dword_17CE758, Buffer); /*0x44215b*/
  }
  if ( dword_17CE8CC ) /*0x442167*/
  {
    if ( !byte_164D0BA ) /*0x442172*/
      --dword_17CE8CC; /*0x44217d*/
  }
  return result; /*0x442183*/
}
```

- Callees: sub_442F60¡ú`th08::GameWindow::ResetRenderState`, _sprintf, __mkdir, sub_443B60¡ú`th08::AnmManager::TakeScreencaptures`, sub_443AF0¡ú`th08::AnmManager::ReleaseSurfaces`, sub_44748F¡ú`th08::Supervisor::TakeSnapshot`

- Bytes: `0x55 0x8b 0xec 0x81 0xec 0x8 0x1 0x0 0x0 0x6a 0x0 0x6a 0x0 0x6a 0x0 0x6a 0x0 0xa1 0x60 0xe7 0x7c 0x1 0x8b 0x8 0x8b 0x15 0x60 0xe7 0x7c 0x1 0x52 0xff 0x51 0x3c 0x85 0xc0 0x7d 0x30 0x8b 0xd 0x90 0xdc 0x...`

---

## th08::AnmManager::DrawVmTextFmt @ 0x4663b0 (size=0xee) (currently 98.6%)

- Sig: `u8 AnmManager::DrawVmTextFmt(...)` [__cdecl]
- Hex-Rays:

```c
int sub_4663B0(int a1, int a2, COLORREF color, int a4, char *Format, ...)
{
  int result; // eax
  int v6; // [esp+0h] [ebp-8Ch]
  char Buffer[132]; // [esp+4h] [ebp-88h] BYREF
  va_list ArgList; // [esp+88h] [ebp-4h]
  va_list va; // [esp+A8h] [ebp+1Ch] BYREF

  va_start(va, Format);
  v6 = *(unsigned __int8 *)(a2 + 664); /*0x4663c3*/
  va_copy(ArgList, va); /*0x4663cc*/
  vsprintf(Buffer, Format, va); /*0x4663de*/
  ArgList = 0; /*0x4663e6*/
  sub_466310( /*0x466480*/
    *(_DWORD *)(*(_DWORD *)(a2 + 548) + 4),
    (unsigned __int64)*(float *)(*(_DWORD *)(a2 + 548) + 8),
    (unsigned __int64)*(float *)(*(_DWORD *)(a2 + 548) + 12),
    (unsigned __int64)*(float *)(*(_DWORD *)(a2 + 548) + 28),
    (unsigned __int64)*(float *)(*(_DWORD *)(a2 + 548) + 24),
    v6,
    *(unsigned __int8 *)(a2 + 665),
    color,
    a4,
    Buffer,
    *(float *)(*(_DWORD *)(a2 + 548) + 56),
    *(float *)(*(_DWORD *)(a2 + 548) + 60));
  result = *(_DWORD *)(a2 + 504) | 1; /*0x46648e*/
  *(_DWORD *)(a2 + 504) = result; /*0x466494*/
  return result; /*0x46649a*/
}
```

- Callees: _vsprintf, sub_466310¡ú`FUN_00466310`, __ftol2

- Bytes: `0x55 0x8b 0xec 0x81 0xec 0x8c 0x0 0x0 0x0 0x8b 0x45 0xc 0xf 0xb6 0x88 0x98 0x2 0x0 0x0 0x89 0x8d 0x74 0xff 0xff 0xff 0x8d 0x55 0x1c 0x89 0x55 0xfc 0x8b 0x45 0xfc 0x50 0x8b 0x4d 0x18 0x51 0x8d 0x95 0x7...`

---

## th08::CSoundManager::Initialize @ 0x471730 (size=0x8e) (currently 98.2%)

- Sig: `long CSoundManager::Initialize(...)` [__thiscall]
- Hex-Rays:

```c
HRESULT __thiscall sub_471730(LPDIRECTSOUND8 *this, HWND a2, DWORD a3, __int16 a4, int a5, __int16 a6)
{
  HRESULT v8; // [esp+4h] [ebp-8h]
  int v9; // [esp+4h] [ebp-8h]

  if ( *this ) /*0x471743*/
  {
    (*this)->lpVtbl->Release(*this); /*0x471755*/
    *this = 0; /*0x47175b*/
  }
  v8 = DirectSoundCreate8(0, this, 0); /*0x47176e*/
  if ( v8 < 0 ) /*0x471775*/
    return v8; /*0x471777*/
  v9 = (*this)->lpVtbl->SetCooperativeLevel(*this, a2, a3); /*0x471794*/
  if ( v9 < 0 ) /*0x47179b*/
    return v9; /*0x47179d*/
  sub_4717C0(a4, a5, a6); /*0x4717b1*/
  return 0; /*0x4717b8*/
}
```

- Callees: DirectSoundCreate8, sub_4717C0¡ú`th08::CSoundManager::SetPrimaryBufferFormat`

- Bytes: `0x55 0x8b 0xec 0x83 0xec 0xc 0x89 0x4d 0xf4 0xc7 0x45 0xfc 0x0 0x0 0x0 0x0 0x8b 0x45 0xf4 0x83 0x38 0x0 0x74 0x19 0x8b 0x4d 0xf4 0x8b 0x11 0x8b 0x45 0xf4 0x8b 0x8 0x8b 0x12 0x51 0xff 0x52 0x8 0x8b 0x4...`

---

## th08::Supervisor::LoadDat @ 0x4461a0 (size=0x92) (currently 97.7%)

- Sig: `ZunResult Supervisor::LoadDat(...)` [__stdcall]
- Hex-Rays:

```c
int sub_4461A0()
{
  char Buffer[132]; // [esp+0h] [ebp-88h] BYREF
  int v2; // [esp+84h] [ebp-4h]

  if ( (unsigned __int8)sub_4748C0("th08.dat") ) /*0x4461b3*/
  {
    sprintf(Buffer, "th08_%.4x%c.ver", 256, 100); /*0x4461d2*/
    dword_17CEAB8 = sub_43E660(0); /*0x4461ea*/
    dword_17CEAB4 = v2; /*0x4461f2*/
    if ( dword_17CEAB8 ) /*0x4461fe*/
    {
      return 0; /*0x44622e*/
    }
    else
    {
      sub_43EB60((int)&unk_164D540, aError_10, Buffer[0]); /*0x44620a*/
      return -1; /*0x446211*/
    }
  }
  else
  {
    sub_43EB60((int)&unk_164D540, aError_11, Buffer[0]); /*0x446222*/
    return -1; /*0x446229*/
  }
}
```

- Callees: sub_4748C0¡ú`th08::PbgArchive::Load`, _sprintf, sub_43E660¡ú`th08::FileSystem::OpenFile`, sub_43EB60¡ú`th08::GameErrorContext::Fatal`

- Bytes: `0x55 0x8b 0xec 0x81 0xec 0x88 0x0 0x0 0x0 0x68 0xc4 0x66 0x4b 0x0 0xb9 0x88 0xf5 0x64 0x1 0xe8 0x8 0xe7 0x2 0x0 0xf 0xb6 0xc0 0x85 0xc0 0x74 0x59 0x6a 0x64 0x68 0x0 0x1 0x0 0x0 0x68 0xb4 0x66 0x4b 0x0...`

---

## th08::Ending::ReadEndFileParameter @ 0x428890 (size=0x73) (currently 97.2%)

- Sig: `i32 Ending::ReadEndFileParameter(...)` [__thiscall]
- Hex-Rays:

```c
int __thiscall sub_428890(const char **this)
{
  int v3; // [esp+4h] [ebp-4h]

  v3 = j__atol(*(this + 2733)); /*0x4288ab*/
  while ( **(this + 2733) ) /*0x4288bc*/
    ++*(this + 2733); /*0x4288cd*/
  while ( !**(this + 2733) ) /*0x4288e3*/
    ++*(this + 2733); /*0x4288f4*/
  return v3; /*0x4288ff*/
}
```

- Callees: j__atol

- Bytes: `0x55 0x8b 0xec 0x83 0xec 0x8 0x89 0x4d 0xf8 0x8b 0x45 0xf8 0x8b 0x88 0xb4 0x2a 0x0 0x0 0x51 0xe8 0x22 0xbb 0x7 0x0 0x83 0xc4 0x4 0x89 0x45 0xfc 0x8b 0x55 0xf8 0x8b 0x82 0xb4 0x2a 0x0 0x0 0xf 0xbe 0x8 ...`

---

## th08::Player::DeletedCallback @ 0x44dc60 (size=0xa4) (currently 96.6%)

- Sig: `ZunResult Player::DeletedCallback(...)` [__fastcall]
- Hex-Rays:

```c
int __thiscall sub_44DC60(void *this)
{
  if ( sub_4338C0(this) ) /*0x44dc67*/
  {
    sub_466060(5); /*0x44dc7c*/
    sub_4070B0(99); /*0x44dc88*/
    sub_422BB0(0, 99); /*0x44dc96*/
    sub_422BB0(1, 99); /*0x44dca4*/
    sub_422BB0(2, 99); /*0x44dcb2*/
    if ( dword_18B896C ) /*0x44dcbe*/
    {
      sub_40B8A0(dword_18B896C); /*0x44dccb*/
      dword_18B896C = 0; /*0x44dcd0*/
    }
    if ( dword_18B8970 ) /*0x44dce1*/
    {
      sub_40B8A0(dword_18B8970); /*0x44dcef*/
      dword_18B8970 = 0; /*0x44dcf4*/
    }
  }
  return 0; /*0x44dd00*/
}
```

- Callees: sub_4070B0¡ú`th08::AsciiManager::SetInterrupt`, sub_422BB0¡ú`th08::AsciiManager::FUN_00422bb0`, sub_4338C0¡ú`FUN_004338c0`, sub_40B8A0¡ú`th08::ZunMemory::Free`, sub_466060¡ú`th08::AnmManager::ReleaseAnm`

- Bytes: `0x55 0x8b 0xec 0x51 0x89 0x4d 0xfc 0xe8 0x54 0x5c 0xfe 0xff 0x85 0xc0 0xf 0x84 0x8a 0x0 0x0 0x0 0x6a 0x5 0x8b 0xd 0x90 0xdc 0x8b 0x1 0xe8 0xdf 0x83 0x1 0x0 0x6a 0x63 0xb9 0x20 0xce 0x4c 0x0 0xe8 0x23 ...`

---

## D3DXVECTOR3::operator+ @ 0x409080 (size=0x46) (currently 96.4%)

- Sig: `_D3DVECTOR* ::operator+(...)` [__thiscall]
- Hex-Rays:

```c
int __thiscall sub_409080(float *this, int a2, float *a3)
{
  float v4; // [esp+0h] [ebp-10h]
  float v5; // [esp+4h] [ebp-Ch]
  float v6; // [esp+8h] [ebp-8h]

  v6 = *(this + 2) + a3[2]; /*0x409094*/
  v5 = *(this + 1) + a3[1]; /*0x4090a4*/
  v4 = *this + *a3; /*0x4090b2*/
  _lambda_3360e8f09260526cc68b047d56218705_::_lambda_3360e8f09260526cc68b047d56218705_( /*0x4090b8*/
    LODWORD(v4),
    LODWORD(v5),
    LODWORD(v6));
  return a2; /*0x4090c0*/
}
```

- Callees: ??0_lambda_3360e8f09260526cc68b047d56218705_@@QAE@ACIAAIAAV?$single_assignment@I@Concurrency@@@Z

- Bytes: `0x55 0x8b 0xec 0x51 0x89 0x4d 0xfc 0x8b 0x45 0xfc 0x8b 0x4d 0xc 0xd9 0x40 0x8 0xd8 0x41 0x8 0x51 0xd9 0x1c 0x24 0x8b 0x55 0xfc 0x8b 0x45 0xc 0xd9 0x42 0x4 0xd8 0x40 0x4 0x51 0xd9 0x1c 0x24 0x8b 0x4d 0...`

---

## D3DXVECTOR3::operator- @ 0x4090d0 (size=0x46) (currently 96.4%)

- Sig: `_D3DVECTOR* ::operator-(...)` [__thiscall]
- Hex-Rays:

```c
int __thiscall sub_4090D0(float *this, int a2, float *a3)
{
  float v4; // [esp+0h] [ebp-10h]
  float v5; // [esp+4h] [ebp-Ch]
  float v6; // [esp+8h] [ebp-8h]

  v6 = *(this + 2) - a3[2]; /*0x4090e4*/
  v5 = *(this + 1) - a3[1]; /*0x4090f4*/
  v4 = *this - *a3; /*0x409102*/
  _lambda_3360e8f09260526cc68b047d56218705_::_lambda_3360e8f09260526cc68b047d56218705_( /*0x409108*/
    LODWORD(v4),
    LODWORD(v5),
    LODWORD(v6));
  return a2; /*0x409110*/
}
```

- Callees: ??0_lambda_3360e8f09260526cc68b047d56218705_@@QAE@ACIAAIAAV?$single_assignment@I@Concurrency@@@Z

- Bytes: `0x55 0x8b 0xec 0x51 0x89 0x4d 0xfc 0x8b 0x45 0xfc 0x8b 0x4d 0xc 0xd9 0x40 0x8 0xd8 0x61 0x8 0x51 0xd9 0x1c 0x24 0x8b 0x55 0xfc 0x8b 0x45 0xc 0xd9 0x42 0x4 0xd8 0x60 0x4 0x51 0xd9 0x1c 0x24 0x8b 0x4d 0...`

---

## th08::CSound::SetVolume @ 0x472840 (size=0x94) (currently 95.8%)

- Sig: `void CSound::SetVolume(...)` [__thiscall]
- Hex-Rays:

```c
int __thiscall sub_472840(_DWORD ***this, int a2)
{
  double v2; // st7
  float v4; // [esp+8h] [ebp-4h]
  float v5; // [esp+8h] [ebp-4h]
  float v6; // [esp+8h] [ebp-4h]
  float v7; // [esp+8h] [ebp-4h]

  if ( !dword_18BDC80 ) /*0x47285f*/
    return (*(int (__stdcall **)(_DWORD, int))(***(this + 1) + 60))(**(this + 1), -10000); /*0x4728cb*/
  v4 = (double)dword_18BDC80 / 100.0; /*0x472855*/
  v2 = 1.0 - v4; /*0x472867*/
  v5 = v2; /*0x47286a*/
  v6 = v2 * v5; /*0x472870*/
  v7 = 1.0 - v6; /*0x47287c*/
  return (*(int (__stdcall **)(_DWORD, _DWORD))(***(this + 1) + 60))( /*0x4728ce*/
           **(this + 1),
           (unsigned __int64)((double)(a2 + 5000) * v7) - 5000);
}
```

- Callees: __ftol2

- Bytes: `0x55 0x8b 0xec 0x83 0xec 0xc 0x89 0x4d 0xf8 0xdb 0x5 0x80 0xdc 0x8b 0x1 0xd8 0x35 0x80 0x49 0x4b 0x0 0xd9 0x5d 0xfc 0x83 0x3d 0x80 0xdc 0x8b 0x1 0x0 0x74 0x52 0xd9 0x5 0x38 0x43 0x4b 0x0 0xd8 0x65 0xf...`

---

## th08::AnmManager::CreateEmptyTexture @ 0x4657c0 (size=0x38) (currently 95.7%)

- Sig: `u32 AnmManager::CreateEmptyTexture(...)` [__thiscall]
- Hex-Rays:

```c
int __stdcall sub_4657C0(int a1, int a2, int a3, int a4)
{
  sub_47AA8D(dword_17CE760, a2, a3, 1, 0, dword_4B8480[a4], 1, a1); /*0x4657eb*/
  return 0; /*0x4657f2*/
}
```

- Callees: sub_47AA8D¡ú`FUN_0047aa8d`

- Bytes: `0x55 0x8b 0xec 0x51 0x89 0x4d 0xfc 0x8b 0x45 0x8 0x50 0x6a 0x1 0x8b 0x4d 0x14 0x8b 0x14 0x8d 0x80 0x84 0x4b 0x0 0x52 0x6a 0x0 0x6a 0x1 0x8b 0x45 0x10 0x50 0x8b 0x4d 0xc 0x51 0x8b 0x15 0x60 0xe7 0x7c 0...`

---

# C. Ending (fresh module)

## th08::Ending::ReadEndFileParameter @ 0x428890 (size=0x73) (currently 97.2%)

- Sig: `i32 Ending::ReadEndFileParameter(...)` [__thiscall]
- Hex-Rays:

```c
int __thiscall sub_428890(const char **this)
{
  int v3; // [esp+4h] [ebp-4h]

  v3 = j__atol(*(this + 2733)); /*0x4288ab*/
  while ( **(this + 2733) ) /*0x4288bc*/
    ++*(this + 2733); /*0x4288cd*/
  while ( !**(this + 2733) ) /*0x4288e3*/
    ++*(this + 2733); /*0x4288f4*/
  return v3; /*0x4288ff*/
}
```

- Callees: j__atol

- Bytes: `0x55 0x8b 0xec 0x83 0xec 0x8 0x89 0x4d 0xf8 0x8b 0x45 0xf8 0x8b 0x88 0xb4 0x2a 0x0 0x0 0x51 0xe8 0x22 0xbb 0x7 0x0 0x83 0xc4 0x4 0x89 0x45 0xfc 0x8b 0x55 0xf8 0x8b 0x82 0xb4 0x2a 0x0 0x0 0xf 0xbe 0x8 ...`

---

## th08::Ending::OnUpdate @ 0x429860 (size=0x8d) (currently 30.8%)

- Sig: `ChainCallbackResult Ending::OnUpdate(...)` [__fastcall]
- Hex-Rays:

```c
int __thiscall sub_429860(_DWORD *this)
{
  int j; // [esp+4h] [ebp-8h]
  int i; // [esp+8h] [ebp-4h]

  for ( i = 0; ; ++i ) /*0x429869*/
  {
    if ( sub_428B80(this) ) /*0x429873*/
      return 0; /*0x42987e*/
    for ( j = 0; j < 15; ++j ) /*0x429880*/
      sub_45EA00(this + 169 * j + 5); /*0x4298af*/
    if ( !*(this + 2710) || (word_164D528 & 0x100) == 0 || i >= 8 ) /*0x4298d7*/
      break; /*0x4298d7*/
  }
  return 1; /*0x4298e9*/
}
```

- Callees: sub_428B80¡ú`th08::Ending::ParseEndFile`, sub_45EA00¡ú`th08::AnmManager::ExecuteScript`

- Bytes: `0x55 0x8b 0xec 0x83 0xec 0xc 0x89 0x4d 0xf4 0xc7 0x45 0xfc 0x0 0x0 0x0 0x0 0x8b 0x4d 0xf4 0xe8 0x8 0xf3 0xff 0xff 0x85 0xc0 0x74 0x4 0x33 0xc0 0xeb 0x69 0xc7 0x45 0xf8 0x0 0x0 0x0 0x0 0xeb 0x9 0x8b 0x...`

---

## th08::Ending::DeletedCallback @ 0x429d40 (size=0x93) (currently 30.8%)

- Sig: `ZunResult Ending::DeletedCallback(...)` [__fastcall]
- Hex-Rays:

```c
int __thiscall sub_429D40(int this)
{
  sub_466060(24); /*0x429d51*/
  dword_17CE8B4 = 6; /*0x429d56*/
  sub_466AE0(0); /*0x429d68*/
  sub_40B8A0(*(void **)(this + 10836)); /*0x429d7c*/
  sub_43CF10(*(_DWORD *)(this + 4)); /*0x429d8d*/
  *(_DWORD *)(this + 4) = 0; /*0x429d95*/
  unknown_libname_4(this); /*0x429da5*/
  j__free((void *)this); /*0x429db4*/
  dword_17CE8D0 = 0; /*0x429dc3*/
  return 0; /*0x429dcf*/
}
```

- Callees: unknown_libname_4, j__free, sub_466AE0¡ú`th08::AnmManager::ReleaseSurface`, sub_43CF10¡ú`th08::Chain::Cut`, sub_40B8A0¡ú`th08::ZunMemory::Free`, sub_466060¡ú`th08::AnmManager::ReleaseAnm`

- Bytes: `0x55 0x8b 0xec 0x83 0xec 0x8 0x89 0x4d 0xf8 0x6a 0x18 0x8b 0xd 0x90 0xdc 0x8b 0x1 0xe8 0xa 0xc3 0x3 0x0 0xc7 0x5 0xb4 0xe8 0x7c 0x1 0x6 0x0 0x0 0x0 0x6a 0x0 0x8b 0xd 0x90 0xdc 0x8b 0x1 0xe8 0x73 0xcd ...`

---

# D. ScoreDat (fresh module)

## th08::ScoreDat::ReleaseScore @ 0x45afc0 (size=0x35) (currently 66.7%)

- Sig: `void ScoreDat::ReleaseScore(...)` [__fastcall]
- Hex-Rays:

```c
int __thiscall sub_45AFC0(void **this)
{
  sub_45A5A0(*(this + 3)); /*0x45afcd*/
  sub_40B8A0(*(this + 3)); /*0x45afde*/
  return sub_40B8A0(this); /*0x45aff1*/
}
```

- Callees: sub_45A5A0¡ú`th08::ScoreDat::FreeAllScores`, sub_40B8A0¡ú`th08::ZunMemory::Free`

- Bytes: `0x55 0x8b 0xec 0x51 0x89 0x4d 0xfc 0x8b 0x45 0xfc 0x8b 0x48 0xc 0xe8 0xce 0xf5 0xff 0xff 0x8b 0x4d 0xfc 0x8b 0x51 0xc 0x52 0xb9 0x98 0xf5 0x64 0x1 0xe8 0xbd 0x8 0xfb 0xff 0x8b 0x45 0xfc 0x50 0xb9 0x98...`

---

## th08::ScoreDat::FreeAllScores @ 0x45a5a0 (size=0x3b) (currently 36.4%)

- Sig: `u8 ScoreDat::FreeAllScores(...)` [__fastcall]
- Hex-Rays:

```c
int __thiscall sub_45A5A0(_DWORD *this)
{
  int result; // eax
  _DWORD *Block; // [esp+0h] [ebp-8h]
  _DWORD *v3; // [esp+4h] [ebp-4h]

  result = (int)this; /*0x45a5a9*/
  for ( Block = (_DWORD *)*(this + 1); Block; Block = v3 ) /*0x45a5af*/
  {
    v3 = (_DWORD *)Block[1]; /*0x45a5be*/
    result = sub_40B8A0(Block); /*0x45a5ca*/
  }
  return result; /*0x45a5d7*/
}
```

- Callees: sub_40B8A0¡ú`th08::ZunMemory::Free`

- Bytes: `0x55 0x8b 0xec 0x83 0xec 0x8 0x89 0x4d 0xf8 0x8b 0x45 0xf8 0x8b 0x48 0x4 0x89 0x4d 0xf8 0x83 0x7d 0xf8 0x0 0x74 0x1f 0x8b 0x55 0xf8 0x8b 0x42 0x4 0x89 0x45 0xfc 0x8b 0x4d 0xf8 0x51 0xb9 0x98 0xf5 0x64...`

---

