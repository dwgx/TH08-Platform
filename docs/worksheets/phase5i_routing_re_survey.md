# Phase 5i Routing RE Survey

Scope: read-only IDA survey for TH08 player-routing work. The first IDA helper call in this run was `server_health`; IDA reported `auto_analysis_ready=true`, `hexrays_ready=true`, image base `0x400000`.

RVA convention: `RVA = VA - 0x400000`.

Tooling note: all IDA calls were made through `python tools/ida_helper.py call <fn> '<json>'`. Early bad-parameter attempts were corrected; no helper endpoint exhausted the max-3 retry budget.

Symbol-address note: the prompt-provided verified addresses were used for player layout and known Player functions. IDA/reccmp function symbols were used for containing-function names and xrefs. The repo/IDA symbols observed for manager singleton addresses differ from the prompt for BulletManager and EnemyManager, so address-specific manager conclusions below are based on function xrefs and code flow, not singleton-offset assumptions.

## Section 1 — Bullet/laser-vs-player damage paths

### Player::TestHitbox

- Address: `0x449FF0`, RVA `0x49FF0`
- Size: `0x233`
- IDA name: `sub_449FF0`
- Mapping name: `Player::TestHitbox`
- Calling convention: `__thiscall(this=Player*)`; call sites load `ecx` with `g_Player` or a Player pointer before calling.
- Prototype shape from Hex-Rays: `int __thiscall sub_449FF0(Player *this, float *pos, int meta)`.
- Return convention: `2` means hit.

Behavior:

- Iterates 192 player hit-test slots at approximately `Player + 0xBB834`, stride `0x40`.
- Skips inactive slots by slot flag at `+0x3C`.
- Tests circle, rotated rectangle, or axis-aligned rectangle depending on slot metadata.
- On hit, writes a slot-derived value to `Player + 0xE2A90`, increments the slot hit count at slot `+0x30`, and returns `2`.
- Does not itself kill the player, play death sounds, clear bullets, or award items. Callers act on the return value.

Representative caller chain and call sites:

| Call VA | RVA | Containing function | Context |
|---:|---:|---|---|
| `0x4308A3` | `0x308A3` | `sub_430830` | Bullet cancel/clear helper; loops active bullet records and tests each against `g_Player`. Hit return is used before spawning/clearing bullet-derived items. |
| `0x4308C3` | `0x308C3` | `sub_430830` | Same helper, second hit-test branch in the active bullet loop. |
| `0x430B19` | `0x30B19` | `sub_430AA0` | Bomb/cancel scoring path; tests bullets against player hitbox while converting/clearing projectiles. |
| `0x4317CF` | `0x317CF` | `BulletManager::OnUpdate` (`sub_431240`) | Main 1536-bullet update loop, bullet state case 2; raw `TestHitbox`, then marks bullet byte `+0xDBE=1` on hit. |
| `0x4318E1` | `0x318E1` | `BulletManager::OnUpdate` (`sub_431240`) | Main bullet update loop, state case 3; raw `TestHitbox`, then hit marker. |
| `0x4319F2` | `0x319F2` | `BulletManager::OnUpdate` (`sub_431240`) | Main bullet update loop, state case 4; raw `TestHitbox`, then hit marker. |
| `0x44A261` | `0x4A261` | `sub_44A230` | Player bullet-body/death wrapper calls `TestHitbox` first. |
| `0x44A4A1` | `0x4A4A1` | `sub_44A470` | Player expanded bullet/graze wrapper calls `TestHitbox` first. |

Side effects on hit:

- Local to `Player::TestHitbox`: writes `Player + 0xE2A90`, increments per-slot hit counter, returns `2`.
- Caller side effects:
  - `BulletManager::OnUpdate` raw state cases mark a bullet hit byte.
  - `sub_430830`/`sub_430AA0` may clear/cancel bullets and spawn score/item effects.
  - `sub_44A230` and `sub_44A470` perform death/graze follow-up behavior.

Recommended hook strategy: **mirror**, with projectile-consumption guardrails.

Justification: bullet damage is a per-player spatial event, so P1 and P2 both need to be tested. The existing 5c hook already covers this primitive, but callers that consume a projectile or spawn item conversions must not double-clear or double-award a single projectile unless that behavior is explicitly desired.

### sub_44A230 — bullet body hit/death wrapper

- Address: `0x44A230`, RVA `0x4A230`
- Size: `0x12E`
- IDA name: `sub_44A230`
- Calling convention: `__thiscall(this=Player*)`; observed caller uses `mov ecx, offset g_Player; call sub_44A230`.
- Prototype shape from Hex-Rays: `int __thiscall sub_44A230(Player *this, bullet_like *bullet)`.

Behavior:

- Calls `Player::TestHitbox(this, bullet_pos, bullet_meta)` first.
- If `TestHitbox` returns nonzero, returns `2`.
- Otherwise tests bullet AABB against player AABB-like fields around `Player + 0x38C`, `+0x390`, `+0x398`, `+0x39C`.
- On overlap, sets GUI/status flag `*(word *)(dword_18B8A28 + 218) |= 2`.
- If player state byte is alive (`0`), calls `sub_44E160(&g_GameManager)` and `sub_44AB40(this)` to process player death, then returns `1`.
- If player is already nonzero state, returns `1` without death processing.

Representative caller chain and call sites:

| Call VA | RVA | Containing function | Context |
|---:|---:|---|---|
| `0x4316B1` | `0x316B1` | `BulletManager::OnUpdate` (`sub_431240`) | Main 1536-bullet loop, state case 1; calls this wrapper after movement/update work for active bullets. |
| `0x44A261` | `0x4A261` | `sub_44A230` | Internal call to `Player::TestHitbox` before AABB/death branch. |

Side effects on hit:

- On `TestHitbox` return: no local death side effect; returns `2` and lets caller decide.
- On AABB overlap: sets GUI/status flag, triggers game-manager hit handling, and calls the player death handler `sub_44AB40`.
- `sub_44AB40` logs `"player DEAD"`, sets player state to `2`, plays death/effects, updates life/bomb/pre-death counters, and calls `sub_441530(g_ItemManager)` to cancel item auto-collect.

Recommended hook strategy: **mirror**.

Justification: this is an actual player damage/death path. P2 must be able to die from the same bullet body/AABB overlap even when P1 does not. Because `sub_44AB40(this)` is Player-pointer based but also touches shared game globals, the mirror must audit shared life/bomb/score behavior before allowing both players to die from one projectile in the same frame.

### sub_44A470 — expanded bullet/graze wrapper

- Address: `0x44A470`, RVA `0x4A470`
- Size: `0x12F`
- IDA name: `sub_44A470`
- Calling convention: `__thiscall(this=Player*)`; observed caller uses `mov ecx, offset g_Player; call sub_44A470`.
- Prototype shape from Hex-Rays: `int __thiscall sub_44A470(Player *this, bullet_like *bullet)`.

Behavior:

- Calls `Player::TestHitbox(this, bullet_pos, bullet_meta)` first.
- If `TestHitbox` returns nonzero, returns `2`.
- Otherwise expands the bullet AABB by about `20.0` pixels and compares against a different player near-box around `Player + 0x3A4..0x3B4`.
- If player state is `1` or `2`, returns `0`.
- On near-overlap, calls `sub_44A930(bullet_or_pos, 0)` and returns `1`.

Representative caller chain and call sites:

| Call VA | RVA | Containing function | Context |
|---:|---:|---|---|
| `0x4315F2` | `0x315F2` | `BulletManager::OnUpdate` (`sub_431240`) | Main 1536-bullet loop, state case 1; performs expanded near-bullet/graze style test. |
| `0x44A4A1` | `0x4A4A1` | `sub_44A470` | Internal call to `Player::TestHitbox`. |

Side effects on hit/near-hit:

- On `TestHitbox` return: returns `2` to caller.
- On expanded near-overlap: calls `sub_44A930`, which updates graze/score-like counters, spawns an effect (`sub_425430`), plays sound `6`, sets GUI flag `0x2000`, and may spawn a time/item type `10` through `ItemManager::SpawnItem` under stage/condition checks.

Recommended hook strategy: **mirror**, with shared-score policy decided explicitly.

Justification: this is per-player spatial graze/near-hit logic, and P2 should be able to graze or trigger the same near-hit feedback. The side effects are global-score and global-effect heavy, so the hook should either intentionally allow two players to graze the same projectile or add a per-projectile/per-frame de-duplication policy.

### Player::CalcLaserHitbox

- Address: `0x44A6A0`, RVA `0x4A6A0`
- Size: `0x28B`
- IDA name: `sub_44A6A0`
- Mapping name: `Player::CalcLaserHitbox`
- Calling convention: `__thiscall(this=Player*)`; call sites load `ecx = g_Player` before calling.
- Prototype shape from Hex-Rays: `int __thiscall sub_44A6A0(Player *this, float *laser_pos, int meta, float *laser_size, float angle, int allow_graze)`.

Behavior:

- Builds a rotated player/laser overlap test from player fields around `this + 173` floats and `this + 245` floats, the laser position/size, and `angle`.
- Direct-hit branch:
  - Sets GUI/status flag `|=2`.
  - If player state is `0`, calls `sub_44E160(&g_GameManager)` and `sub_44AB40(this)`.
  - Returns `1` for direct hit when alive; returns `0` if state already nonzero.
- Graze branch:
  - If no direct hit and `allow_graze != 0`, expands laser AABB by about `48.0`.
  - If player state is not `1` or `2`, calls `sub_44A930(this + 173, 1)`.
  - Returns `2` for graze/near-hit.
- Otherwise returns `0`.

Representative caller chain and call sites:

| Call VA | RVA | Containing function | Context |
|---:|---:|---|---|
| `0x4247EC` | `0x247EC` | `sub_424730` | ECL laser helper wrapper; calls `CalcLaserHitbox` with `g_Player`, likely graze-enabled pass. |
| `0x42480B` | `0x2480B` | `sub_424730` | Same wrapper; second call, likely direct-hit pass. |
| `0x4248DC` | `0x248DC` | `sub_424820` | ECL laser helper wrapper variant; first laser hitbox call. |
| `0x4248FB` | `0x248FB` | `sub_424820` | Same wrapper; second laser hitbox call. |
| `0x4249CC` | `0x249CC` | `sub_424910` | ECL laser helper wrapper variant; first laser hitbox call. |
| `0x4249EB` | `0x249EB` | `sub_424910` | Same wrapper; second laser hitbox call. |
| `0x431EA1` | `0x31EA1` | `BulletManager::OnUpdate` (`sub_431240`) | Laser loop over 256 laser records at manager offset around `+0x660938`, state case 0. |
| `0x431F3B` | `0x31F3B` | `BulletManager::OnUpdate` (`sub_431240`) | Laser loop, state case 1. |
| `0x43208A` | `0x3208A` | `BulletManager::OnUpdate` (`sub_431240`) | Laser loop, state case 2. |
| `0x41D4F4` | `0x1D4F4` | `EclManager::RunEcl` (`sub_4184B0`) | Representative caller into the ECL laser wrapper dispatch/table path. |

Side effects on hit:

- Direct hit: GUI/status flag, game-manager hit handling, player death handler.
- Graze: calls `sub_44A930`, which is global-score/effect/sound heavy.
- No evidence that this function clears the laser; it reports hit/graze to laser caller while applying player effects internally.

Recommended hook strategy: **mirror**.

Justification: lasers are continuous spatial hazards and can overlap either player independently. Mirror the test for P1 and P2, but decide how to handle shared graze score/sound/effect duplication if both players graze the same laser in the same frame.

### BulletManager coverage summary

- `BulletManager::OnUpdate` at `0x431240`, RVA `0x31240`, size `0xF16`.
- Main bullet loop: 1536 bullet records, active/state word around bullet `+0xDB8`.
- State case 1 uses `sub_44A470` at `0x4315F2` and `sub_44A230` at `0x4316B1`.
- State cases 2, 3, and 4 use raw `Player::TestHitbox` at `0x4317CF`, `0x4318E1`, and `0x4319F2`, then mark bullet byte `+0xDBE=1` on hit.
- Laser loop: 256 laser records, base around BulletManager `+0x660938`, stride `0x59C`, active flag near `+0x584`, state byte near `+0x598`.
- Laser loop calls `Player::CalcLaserHitbox` at `0x431EA1`, `0x431F3B`, and `0x43208A`.

### EnemyManager coverage summary

- `EnemyManager::OnUpdate` at `0x42C660`, RVA `0x2C660`, size `0x1836`.
- No separate enemy-owned projectile-vs-player damage path was found in `EnemyManager::OnUpdate`.
- EnemyManager does read `g_Player.pos` for targeting/aim and enemy behavior decisions; those xrefs are listed in Section 3.
- Conclusion: bullet/laser damage routing is concentrated in Player hitbox wrappers plus `BulletManager::OnUpdate`; enemy aim routing is separate.

## Section 2 — Item collection (`CalcItemBoxCollision`)

### Player::CalcItemBoxCollision

- Address: `0x44A5A0`, RVA `0x4A5A0`
- Size: `0xF9`
- IDA name: `sub_44A5A0`
- Mapping name: `Player::CalcItemBoxCollision`
- Calling convention: `__thiscall(this=Player*)`; caller loads `ecx = g_Player`.
- Prototype shape from Hex-Rays: `BOOL __thiscall sub_44A5A0(Player *this, item_pos *pos, int meta)`.

Behavior:

- This function is a pure collision predicate. It does not iterate the item list and does not apply item rewards.
- State gate:
  - If player state is neither `0`, `3`, nor `4`, it returns false.
  - Therefore vanilla excludes states `1` and `2`, but allows states `3` and `4`.
  - This differs from the requested multiplayer routing rule, where state `2` dead and state `3` respawn-ghost must not be eligible for P2 item collection.
- Collision shape:
  - Builds a small item AABB around the item position, with apparent half-size around `2.0`.
  - Compares against the player's item collection AABB at verified offsets:
    - min x: `Player + 0x3BC`
    - min y: `Player + 0x3C0`
    - max x: `Player + 0x3C8`
    - max y: `Player + 0x3CC`
  - Returns true on AABB overlap.

Caller chain:

| Call VA | RVA | Containing function | Context |
|---:|---:|---|---|
| `0x440A17` | `0x40A17` | `ItemManager::OnUpdate` (`sub_440500`) | Per-active-item collision check; calls `CalcItemBoxCollision(g_Player, item.currentPosition, ...)` before item reward switch. |
| `0x43127B` | `0x3127B` | `BulletManager::OnUpdate` (`sub_431240`) | Calls `ItemManager::OnUpdate` early in the frame, so bullet update is part of the observed caller chain into item processing. |

### ItemManager::OnUpdate behavior

- Address: `0x440500`, RVA `0x40500`
- Size: `0x7C5`
- IDA name: `sub_440500`
- Mapping name: `ItemManager::OnUpdate`

Iteration and layout:

- Iterates a linked list of item records from the manager list head.
- Item record size observed around `0x2E4`.
- Key fields:
  - `currentPosition`: item `+0x2A4`
  - velocity: item `+0x2B0`
  - item type: item `+0x2D4`
  - `isInUse`: item `+0x2D5`
  - `isOnscreen`: item `+0x2D6`
  - state: item `+0x2D7`
  - `isMaxValue`: item `+0x2D8`
  - next pointer: item `+0x2DC`

Item types handled after collision:

| Type | Meaning | Observed side effects |
|---:|---|---|
| `0` | small power (`P`) | Calls `sub_440CF0`; increments power by 1 through `sub_441850(&GameManager, 1)`; score `+10`; may trigger full-power conversion path `sub_441450`. |
| `1` | point item | Calls `sub_440E40`; score depends on y/max-value state; increments point-item counters around `dword_160F510 + 44/+48`; calls extend-update helper `sub_440470`; may call life extend `sub_439B29`. |
| `2` | big power | Calls `sub_441170`; power `+8`; score `+10`; may trigger full-power conversion. |
| `3` | bomb | If bombs are below cap, increments bombs via `sub_439883`; sets GUI/status bits around `dword_160F42C`; plays item sound. |
| `4` | full power | If power < 128, cancels/collects bullets/items/time effects, sets power to 128, score `+1000`, spawns popup/effects, sets GUI bits. |
| `5` | life/extend | Calls `sub_439B29`. |
| `6` | point star | Adds score based on `dword_18B8974` or formula `10 * (dword_160F510 + 12 / 40) + 300`; shared-score style side effect. |
| `7` | time item | Calls `sub_4412B0`; applies time orb/chain/score side effects. |
| `8` | small point | Calls `sub_441020`; scaled score reward. |

Shared/global side effects:

- `CalcItemBoxCollision` itself has no side effects.
- The item reward switch writes global game state rather than Player-local inventory:
  - score and score popups
  - power through `GameManager`/ZunGlobals style helpers
  - point item counters
  - life/bomb counters
  - GUI/status flags
  - sound index `21` or `44`, depending on `item.isMaxValue`
  - item deletion through `sub_441770(item)`
- Evidence therefore points to vanilla item rewards being globally shared. If P2 collects an item, the same global score/power/life/bomb side effects should still run unless the multiplayer DLL deliberately introduces separate P2 accounting.

### Item-attract behavior

Item attraction is handled in `ItemManager::OnUpdate`, not in `CalcItemBoxCollision`.

Observed behavior:

- `ItemManager::OnUpdate` reads `g_Player.y` at `0x440801` and player state bytes around `0x4406F5`, `0x4407B4`, and `0x440875`.
- In the default free-fall item state, the function checks auto-collect and attraction conditions:
  - player at/above the auto-item line,
  - power and stage-specific conditions,
  - player state gates.
- If attraction starts, the item is moved into state `1`, velocity is adjusted, and vector helpers such as `sub_44C1B0(item.pos)` / `sub_4286E0(...)` are used to direct motion toward the player.
- `sub_4413E0` is an auto-collect-all helper: it sets every item to attraction state `1` with velocity `(0, -0.5, 0)`.
- `sub_441530` cancels auto-collect: it moves state-1 items back to default/free-fall with velocity `(0, -0.9, 0)`. This is called from the player death handler.

Recommended hook strategy: **route by distance**.

Implementation rule:

- For each active item, choose the nearest eligible player using squared distance:
  - `dist2(item.x, item.y, P1.x, P1.y)`
  - `dist2(item.x, item.y, P2.x, P2.y)`
- Eligibility must exclude dead/respawn-ghost P2 states per request:
  - state `2` dead: not eligible
  - state `3` respawn-ghost: not eligible
- Also preserve vanilla non-eligible states unless intentionally changed. Vanilla `CalcItemBoxCollision` excludes state `1` and `2`, and allows `3`/`4`; multiplayer routing should override this only where the desired 2P rule is explicit.
- Call/apply collision and attraction against the selected player only.
- Let global reward side effects run exactly once for the item, regardless of whether P1 or P2 collected it.

Justification: an item is a mutually exclusive pickup and should not be collected twice. Unlike bullets/lasers, the correct 2P behavior is "nearest eligible player wins", while shared score/power/bomb/life side effects remain global through the vanilla reward path.

## Section 3 — Enemy aiming / homing / aimed-shot xrefs

Ground-truth player position addresses used for the scan:

- `g_Player.pos.x = 0x17D61AC` (`g_Player + 0x2B4`)
- `g_Player.pos.y = 0x17D61B0` (`g_Player + 0x2B8`)

Evidence scan:

- Full `.text` immediate-byte scan found 31 hits for `0x17D61AC`, 14 hits for `0x17D61B0`, and 74 hits for `0x17D5EF8`.
- Non-enemy/UI-only reads were separated from targeting reads. Excluded examples include ASCII/debug draw helpers around `0x402E92`, high-priority draw helpers around `0x4054D7..0x405995`, GUI reads around `0x435B94/0x435BA7`, and the item-attract read at `0x440801` covered in Section 2.
- Enemy, ECL, spellcard, homing, and aimed-shot relevant reads found in that scan are listed below.

### Targeting xref table

| Read VA | RVA | Containing function | Role | Read style | Recommended hook strategy |
|---:|---:|---|---|---|---|
| `0x416C11` | `0x16C11` | `Spellcard::SpellcardOnUpdateImpl` (`sub_416B90`) | Spellcard/player-relative position logic; reads player x. | Direct global immediate. | Hook function or local read source to use nearest eligible player. |
| `0x416C24` | `0x16C24` | `Spellcard::SpellcardOnUpdateImpl` (`sub_416B90`) | Same spellcard/player-relative logic; reads player y. | Direct global immediate. | Same as above; keep x/y from same selected player. |
| `0x4173B7` | `0x173B7` | `Spellcard::SpellcardOnUpdateImpl` (`sub_416B90`) | Passes `&g_Player.pos` to vector/math helper for spellcard/familiar target movement. | Direct pointer to global position. | Hook function and substitute pointer/scratch vector for nearest player. |
| `0x417477` | `0x17477` | `Spellcard::SpellcardOnUpdateImpl` (`sub_416B90`) | Second vector/math helper use of `&g_Player.pos`. | Direct pointer to global position. | Hook at containing function; pointer substitution must keep lifetime valid. |
| `0x41E292` | `0x1E292` | `EclManager::RunEcl` (`sub_4184B0`) | ECL opcode case reads `g_Player.x`, likely script-visible aim/position variable. | Direct global immediate. | Prefer central ECL variable resolver hook if this is variable access; otherwise patch this opcode case. |
| `0x41F8D3` | `0x1F8D3` | `sub_41F420` | ECL variable resolver returns/uses player x for variable `0x273D`. | Direct global immediate. | Hook resolver and return nearest selected player's x. |
| `0x41F8E3` | `0x1F8E3` | `sub_41F420` | ECL variable resolver returns/uses player y for variable `0x273E`. | Direct global immediate. | Hook resolver and return y from the same selected player used for x. |
| `0x41FAC2` | `0x1FAC2` | `sub_41F420` | Calls helper with enemy/script position; appears to compute angle/distance to player. | Direct or helper-mediated global use. | Hook resolver/helper at the variable level so all ECL distance/angle variables route consistently. |
| `0x41FAE3` | `0x1FAE3` | `sub_41F420` | Computes vector/distance from enemy to player for ECL variable around `0x2742`. | Direct global position. | Hook resolver and calculate against nearest eligible player. |
| `0x42056B` | `0x2056B` | `sub_420120` | ECL float variable resolver returns player x for variable `0x273D`. | Direct global immediate. | High-value central hook: route `0x273D`/`0x273E` by nearest player. |
| `0x420576` | `0x20576` | `sub_420120` | ECL float variable resolver returns player y for variable `0x273E`. | Direct global immediate. | Same central resolver hook; select once per evaluation context. |
| `0x420759` | `0x20759` | `sub_420120` | Computes distance/vector to `&g_Player.pos` for variable around `0x2742`. | Direct pointer to global position. | Hook resolver and compute using nearest player's position. |
| `0x42076D` | `0x2076D` | `sub_420120` | References `g_Player` via helper `sub_40BC40(&g_Player)`; targeting-adjacent state/persona read rather than pure pos. | Direct pointer to global Player. | Inspect when implementing; if state affects aim, route through selected Player pointer. |
| `0x420B15` | `0x20B15` | `sub_420950` | ECL variable pointer resolver returns pointer to `g_Player.x`. | Direct global address returned as pointer. | Tricky: returning a pointer to a nearest-player scratch vector may be required; caller hook may be safer. |
| `0x420B1F` | `0x20B1F` | `sub_420950` | ECL variable pointer resolver returns pointer to `g_Player.y`. | Direct global address returned as pointer. | Same pointer-lifetime issue; avoid returning stack memory. |
| `0x42202F` | `0x2202F` | `sub_422020` | Enemy bounce/aim-angle helper compares against player x to set angle/side. | Direct global immediate. | Hook function and calculate against nearest player x. |
| `0x422040` | `0x22040` | `sub_422020` | Same helper; second player-x comparison. | Direct global immediate. | Same function-level hook. |
| `0x4224C3` | `0x224C3` | `sub_4224A0` | Enemy side/wrap aim-angle helper reads player x. | Direct global immediate. | Hook function; use nearest player's x consistently across all reads. |
| `0x4224D6` | `0x224D6` | `sub_4224A0` | Same helper; second player-x read. | Direct global immediate. | Same function-level hook. |
| `0x4224EE` | `0x224EE` | `sub_4224A0` | Same helper; third player-x read. | Direct global immediate. | Same function-level hook. |
| `0x422553` | `0x22553` | `sub_4224A0` | Same helper; wrap/side branch player-x read. | Direct global immediate. | Same function-level hook. |
| `0x422565` | `0x22565` | `sub_4224A0` | Same helper; wrap/side branch player-x read. | Direct global immediate. | Same function-level hook. |
| `0x4227AA` | `0x227AA` | `sub_422720` | Enemy bullet spawn setup; distance gate compares enemy/bullet x to player x before spawning. | Direct global immediate. | Hook function; distance gate should use nearest eligible player. |
| `0x4227B9` | `0x227B9` | `sub_422720` | Same spawn gate; player y read. | Direct global immediate. | Same function-level hook; x/y must be paired. |
| `0x4227CA` | `0x227CA` | `sub_422720` | Same spawn gate; second player x read. | Direct global immediate. | Same function-level hook. |
| `0x4227D9` | `0x227D9` | `sub_422720` | Same spawn gate; second player y read. | Direct global immediate. | Same function-level hook. |
| `0x426C5F` | `0x26C5F` | no named function boundary at scan site (`0x426C40` thunk/block) | Copies `g_Player.x`/position into object `this + 0x2A4` if `sub_428720` is false; likely table-called projectile/object target acquisition. | Direct global immediate. | Resolve table caller before patching; likely hook the table target and substitute nearest player position. |
| `0x426C67` | `0x26C67` | same `0x426C40` block | Copies `g_Player.y`/position into object target vector. | Direct global immediate. | Same unresolved table-call hook. |
| `0x42841F` | `0x2841F` | `sub_428310` | Vector subtract with `&g_Player.pos`; enemy/projectile behavior near player. | Direct pointer to global position. | Hook function and pass nearest player position to vector math. |
| `0x42842B` | `0x2842B` | `sub_428310` | Same vector subtract path. | Direct pointer to global position. | Same function-level hook. |
| `0x42D384` | `0x2D384` | `EnemyManager::OnUpdate` (`sub_42C660`) | Computes vector from enemy/boss to player using `&g_Player.pos`. | Direct pointer to global position. | Hook containing branch or helper call; choose nearest player per enemy context. |
| `0x42D3AB` | `0x2D3AB` | `EnemyManager::OnUpdate` (`sub_42C660`) | Second vector computation from enemy/boss to player. | Direct pointer to global position. | Same as above. |
| `0x42D48A` | `0x2D48A` | `EnemyManager::OnUpdate` (`sub_42C660`) | Compares enemy x to player x; if abs delta < `64` and not child enemy, updates `dword_18B89B4`. | Direct global immediate. | Hook branch to compare against nearest eligible player or both players, depending on what `dword_18B89B4` controls. |

### Section 3 conclusions

- Direct global reads dominate: most enemy/ECL targeting code embeds `0x17D61AC`, `0x17D61B0`, or `&g_Player`.
- The best first central hooks are the ECL variable resolvers:
  - `sub_420120` for float variables such as player x/y and player distance.
  - `sub_420950` for pointer-return variables, with careful scratch storage or caller-level substitution.
  - `sub_41F420` if it is a second resolver path used by opcode families not covered by `sub_420120`.
- Helper-level hooks should follow for direct non-ECL aim helpers:
  - `sub_422020`
  - `sub_4224A0`
  - `sub_422720`
  - `sub_428310`
  - `EnemyManager::OnUpdate` branches around `0x42D384`, `0x42D3AB`, and `0x42D48A`
- For every x/y pair, select the target player once and use both coordinates from that same player. Do not independently choose nearest x and nearest y.

Recommended hook strategy: **route by distance** for aiming/homing.

Justification: enemy aim should choose a single target, not fire every aimed shot twice. The default policy should be nearest eligible player at the time of aim computation. Direct global reads can be handled by function-level hooks or local patch shims; pointer-return resolvers need special care because a pointer to temporary nearest-player coordinates must remain valid for the caller's use.

## Section 4 — RUEEE/th06_multi_net design notes

Not performed. The optional public-repo fetch/list-tree pass was skipped after completing the IDA survey so this worksheet does not make claims about `RUEEE/th06_multi_net`.

## Hook implementation roadmap

1. `Player::TestHitbox` audit, `0x449FF0`, strategy **mirror**, complexity **LOW**.
   Existing 5c hook is the foundation. Verify it handles raw `BulletManager::OnUpdate` state cases and does not double-consume bullets in `sub_430830`/`sub_430AA0`.

2. Bullet wrapper hook for `sub_44A230`, `0x44A230`, strategy **mirror**, complexity **MEDIUM**.
   This is the missing bullet-body/death wrapper. It must call the original logic for P1/P2 or reproduce the AABB/death branch while preserving `sub_44AB40(this)` semantics.

3. Bullet graze/near-hit wrapper hook for `sub_44A470`, `0x44A470`, strategy **mirror**, complexity **MEDIUM**.
   This covers expanded bullet near-overlap/graze behavior. Decide whether shared score/effect side effects from `sub_44A930` may occur once per player or once per projectile per frame.

4. Laser hitbox hook for `Player::CalcLaserHitbox`, `0x44A6A0`, strategy **mirror**, complexity **HIGH**.
   This must cover ECL wrapper calls (`0x424730`, `0x424820`, `0x424910`) and `BulletManager::OnUpdate` laser loop calls (`0x431EA1`, `0x431F3B`, `0x43208A`). Direct-hit death and graze score/effects are both inside this function.

5. Item collision router at `ItemManager::OnUpdate` call site `0x440A17`, helper `Player::CalcItemBoxCollision` `0x44A5A0`, strategy **route by nearest eligible player**, complexity **HIGH**.
   Hooking only `CalcItemBoxCollision` is not enough for clean routing because item reward side effects occur in `ItemManager::OnUpdate` after the boolean return. The hook should select one eligible player per item, pass that player to the collision predicate, and let the vanilla reward switch run once.

6. Item attraction router in `ItemManager::OnUpdate`, especially reads around `0x440801` and state transition to item state `1`, strategy **route by nearest eligible player**, complexity **HIGH**.
   Collection and attraction must agree on the target player or items may home to one player and be collected by the other. Preserve `sub_4413E0` auto-collect-all and `sub_441530` cancel behavior.

7. ECL aiming variable resolver hooks: `sub_420120`, `sub_420950`, and likely `sub_41F420`, strategy **route by nearest eligible player**, complexity **MEDIUM/HIGH**.
   Start here for aimed-shot scripts because these centralize player x/y/distance variables. `sub_420950` is harder because it returns pointers; use stable scratch storage or hook callers instead.

8. Direct enemy aim helper hooks: `sub_422020`, `sub_4224A0`, `sub_422720`, and `sub_428310`, strategy **route by nearest eligible player**, complexity **MEDIUM**.
   These are compact helpers with embedded direct reads of `g_Player.pos`. Hook each function and replace all x/y reads with one selected target.

9. EnemyManager targeting branches in `EnemyManager::OnUpdate`, `0x42C660`, especially reads at `0x42D384`, `0x42D3AB`, and `0x42D48A`, strategy **route by nearest eligible player**, complexity **MEDIUM/HIGH**.
   These are broader update-loop branches, so implement after central ECL/helper hooks to avoid duplicating behavior.

10. Spellcard targeting hook for `Spellcard::SpellcardOnUpdateImpl`, `0x416B90`, strategy **route by nearest eligible player**, complexity **MEDIUM**.
    This handles spellcard/familiar vector math that passes `&g_Player.pos` directly.
