# Phase 5 — True 2-Player (真 2P) Design

**Status**: design draft, no code yet.
**Prerequisite verified**: Player struct layout — `&g_Player = 0x17D5EF8`,
`sizeof(Player) = 0xE2B30 (928560 B)`. See
`dll/src/state/player_layout.h` for offsets.

This is the hardest DLL phase. It requires inventing state ZUN never
shipped: a second concurrent `Player` instance in a single TH08 process,
plus all the game systems re-routed to support both.

---

## Architecture goals

1. **`g_Player2`** — a second 928560-byte block, allocated by us, ctor'd
   the same way ZUN ctor's `g_Player` (call `Player::Player(0x449ca0)` on
   our block via assembly thunk).
2. **Bullet collision tests both players** — patch `BulletManager::Update`
   so per-bullet-frame check runs against `g_Player` AND `g_Player2`.
3. **Item attribution** — when an item is collected, decide which player
   gets credit (proximity? pickup line?).
4. **Score / Lives / Bombs split** — currently in `ZunGlobals` (singleton).
   Need either a P2 mirror struct or per-frame routing.
5. **HUD redesign** — mirror P1's bottom-left score/lives/bombs UI to
   bottom-right for P2. Touch `Gui` / `AsciiManager`.
6. **Input routing** — P1 keyboard, P2 controller (or vice versa). Phase
   2's input hook already separates raw input → action; route per-player.

---

## Sub-phase plan

| Sub | Goal | Effort | Risk |
|---|---|---|---|
| **5a** | Allocate g_Player2 + ctor + register in chain | 2-3 days | Low — Player::RegisterChain pattern is reusable |
| **5b** | Hook Player::OnUpdate to update BOTH players per frame | 2-3 days | Medium — need to verify chain dispatcher iterates twice cleanly |
| **5c** | Hook BulletManager bullet-iteration → test both player hitboxes | 3-4 days | High — need to find collision call site, may need to clone bullet record per hit |
| **5d** | Hook ItemManager::OnUpdate item-collection → route to nearest player | 1-2 days | Low — CalcItemBoxCollision already takes Player* |
| **5e** | P2 input routing (split InputHook output) | 1-2 days | Low — Phase 2 already has the hook |
| **5f** | P2 score/lives/bombs storage (mirror ZunGlobals partial) | 2-3 days | Medium — need to find every read site of ZunGlobals.livesRemaining etc. |
| **5g** | P2 HUD (Gui/AsciiManager mirror to right side) | 3-5 days | High — rendering coords are scattered |
| **5h** | End-to-end test (both players visible, hit detection works, score split) | 2-3 days | n/a — integration |

**Total estimate**: 16-25 days = ~3-5 weeks at one-person pace.

---

## Sub-phase 5a — g_Player2 allocation (the first concrete step)

### Approach

In our DLL, allocate a 928560-byte aligned buffer:

```cpp
// dll/src/state/player2.h
namespace th08_platform::player2 {

inline constexpr std::size_t kSize_Player = 0xE2B30;
extern alignas(16) uint8_t g_Player2[kSize_Player];

void Construct();   // call once after game init
void Destruct();    // call once before game shutdown

}  // namespace th08_platform::player2
```

`Construct()` must:
1. Zero the buffer (`memset(g_Player2, 0, kSize_Player)`).
2. Call the original `Player::Player` ctor at `0x449ca0` with `ECX = g_Player2`:
   ```cpp
   using PlayerCtor_t = void(__thiscall*)(void* this_);
   auto ctor = reinterpret_cast<PlayerCtor_t>(0x449ca0);
   ctor(g_Player2);
   ```
3. Register a parallel chain — call `Player::RegisterChain` semantics but
   for `g_Player2`. Easiest: copy the body of `0x44c230` but substitute
   the `&byte_17D5EF8` reference with `g_Player2`. Could also patch the
   chain registration in-place (more invasive).
4. Add `g_Player2` to `g_Chain` so its OnUpdate runs each frame alongside
   `g_Player`.

### Open question

Does TH08's chain dispatcher (`g_Chain.Step`) gracefully handle two
separate Player chain entries, or does it assume singleton? Quick test:
register g_Player2 to chain, see if game crashes / one player renders /
both render. **MUST be answered before sub_5b.**

### 5a status (2026-04-26)

- **Skeleton committed** (`7a413f4`): `state/player2.{h,cpp}` provides
  `Construct()` (zero-fill + SEH-wrapped thiscall ctor) and `Destruct()`.
- **Trigger**: env var `TH08_PLATFORM_TEST_PLAYER2=1` invokes Construct()
  in DLL init thread. Default off.
- **Risk**: ctor at 0x449ca0 calls `sub_406720(this+16)` which inits an
  inline AnmVm. If AnmManager (g_AnmManager at TBD) isn't ready, ctor
  crashes. SEH catches it; g_Player2 stays zeroed.
- **Test plan** (deferred to user): inject DLL with env var set AFTER
  starting the game past title screen. Watch log for
  `player2: g_Player2 constructed at 0x... post-ctor playerState=0`.

### 5b prerequisite (game-init timing)

Verified: `sub_43ABD7` (the gameplay-init function, address 0x43ABD7,
3423 bytes — likely `GameManager::OnGameStart` or similar). Sequence:

1. `dword_160F508 = malloc(...)`             — alloc GameManager  
2. `dword_160F50C = operator new(0x3Cu)`     — alloc 60-byte struct  
3. `dword_160F510 = operator new(0xE4u)`     — alloc ZunGlobals  
4. `sub_43B984()`                            — game state init  
5. `qmemcpy(... &word_17CE870 ...)`          — config copy  
6. **`result = sub_44C230(0);`**             — **Player::RegisterChain**  ← timing point
7. `sub_43C686(&dword_160F508)`              — more chain installs
8. ZunGlobals init: lives, score, gauges...  
9. ... runs until level start

**5b plan**: hook `Player::RegisterChain` at 0x44C230 with MinHook;
trampoline calls original first, then if return == ZUN_SUCCESS, our
detour calls `player2::Construct()` and registers a parallel chain
element via `sub_43CE60(0x44C390)` (chain-elem alloc) + chain-add via
`sub_43C880`/`sub_43C960`. **Skip if `Construct()` returns false** —
don't add a half-init player to the chain.

### Chain primitive (sub_43CE60)

Verified body — allocates a 32-byte ChainElem:

```c
Block = operator new(0x20u);           // 32 = sizeof(ChainElem)
sub_43C760(Block);                     // ChainElem::Init
v4 = sub_429DE0(v2, 32, "funcChainInf"); // tag/track allocation
sub_406520(a1);                        // a1 = function ptr (e.g. 0x44C390)
*(WORD*)(v4 + 2) |= 1;                 // mark as registered
return v4;
```

The returned ChainElem layout (verified in RegisterChain):
- `+0x08` = AddedCallback pointer
- `+0x0C` = DeletedCallback pointer
- `+0x1C` = context pointer (set to `&g_Player` for P1, will be `&g_Player2` for P2)

For 5b, we replicate `sub_44C230` body for g_Player2:
```c
g_Player2CalcChain      = sub_43CE60(0x44C390);  // OnUpdate
g_Player2DrawChainHigh  = sub_43CE60(0x44D530);  // OnDrawHighPrio  
g_Player2DrawChainLow   = sub_43CE60(0x44D630);  // OnDrawLowPrio
*(DWORD*)(g_Player2CalcChain      + 28) = (DWORD)&g_Player2;
*(DWORD*)(g_Player2DrawChainHigh  + 28) = (DWORD)&g_Player2;
*(DWORD*)(g_Player2DrawChainLow   + 28) = (DWORD)&g_Player2;
*(DWORD*)(g_Player2CalcChain      +  8) = 0x44D650;  // AddedCallback
*(DWORD*)(g_Player2CalcChain      + 12) = 0x44DC60;  // DeletedCallback
sub_43C880(g_Player2CalcChain,      9);   // priority 9 same as P1
sub_43C960(g_Player2DrawChainHigh,  9);
sub_43C960(g_Player2DrawChainLow,  10);
```

---

## Sub-phase 5c — Dual collision (the hardest piece)

### Where bullet→player collision lives

Need to find via IDA. Likely call site:
- `BulletManager::OnUpdate` (in mapping.csv as `th08::BulletManager::OnUpdate`)
- Or per-bullet-iter sub-routine inside it

### Strategies

**Strategy 1**: Detour the per-bullet-test function. After original test
against g_Player returns false, call again with g_Player2.
- Pro: minimal patches.
- Con: doubles bullet→player CPU cost (probably negligible).

**Strategy 2**: Patch the collision routine in-place to test both players
in one pass. More efficient, but more invasive.

**Strategy 3**: Maintain g_Player2 as a "shadow" — copy its position into
g_Player before each bullet's test, restore after. Hacky.

**Recommended**: Strategy 1 (clean detour). Fall back to 2 if performance
issue measured.

---

## Sub-phase 5g — HUD (also hard)

### Gui's responsibility

`Gui::OnUpdate` and `Gui::OnDraw` (in mapping.csv) render the score / lives
/ bombs / power / graze UI. Currently 100% byte-matched in decomp.

### P2 HUD options

**Option A**: Mirror the entire P1 HUD to the right side of the screen.
Patch `Gui::OnDraw` to call drawing code twice with shifted coords.
- Pro: simplest visually.
- Con: invasive — Gui::OnDraw byte-match would regress; need to mark it
  as accepted-mismatch in reccmp.

**Option B**: Add P2 HUD via separate AnmManager script/sprites. Don't
touch Gui::OnDraw at all; render via a parallel chain entry.
- Pro: zero modification to existing code.
- Con: requires new sprite assets (or cloning/recoloring P1's via AnmManager
  vm manipulation).

**Recommended**: Option B (cleaner, preserves byte-match). Develop in
parallel with sub-phase 5a.

---

## Risks not yet enumerated

1. **AnmVm script determinism** — Player ctor calls `sub_465A40` to load
   anm script. Two players means two anm sessions. If the anm script has
   global state, both players' anims will desync.
2. **Sound / SFX pooling** — `g_SoundPlayer` is singleton. Two players
   shooting → 2x SFX queue load. Likely fine but verify.
3. **PbgArchive read concurrency** — both players load sht files. Already
   thread-unsafe in ZUN's design but only called once at AddedCallback,
   so probably fine if we serialize the constructions.

---

## When to start

**Path 1 (TH08-first)**: starts immediately after user picks Path 1.
**Path 2 (TH06 first then port)**: this design transfers — sub-phases
5a/5c/5g logic apply to TH06 too. RUEEE/th06_multi_net's
`Connection.cpp` shows how they handled dual-input + delay; we'd port
that to TH08 in this Phase 5.
**Path 3 (decomp first)**: skip Phase 5 until decomp fills more Player
helper FUN_ functions, since 5a/5c need clean Player::OnUpdate semantics.

---

*Author: Claude (Opus 4.7), session 3, 2026-04-25.
References: `dll/src/state/player_layout.h`, `memory/12_player_struct.md`,
RUEEE/th06_multi_net Connection.cpp pattern (per session 3 codex research).*
