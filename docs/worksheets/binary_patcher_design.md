# TH08 Binary Patcher Design

## Goal

Design a post-compile, pre-link COFF patcher for the TH08 decomp that can surgically rewrite rebuilt `.obj` bytes when C/C++ source generation has plateaued. The immediate target is `th08.exe` v1.00d as declared in `reccmp-project.yml`, with SHA256 `330fbdbf58a710829d65277b4f312cfbb38d5448b3df523e79350b879213d924`.

The problem is no longer "make the source look nicer." The problem is "make a very small set of already-understood functions emit the exact byte pattern the original object file used." The patcher therefore should be:

- COFF-aware, not PE-aware.
- Allowlist-driven, not heuristic-first.
- Fixed-length and relocation-safe.
- Inserted between `cl.exe` and `link.exe`, so both the final EXE and `objdiff` output see the patched object.

## Existing Tooling Notes

### `scripts/build.py`

- `build()` calls `configure(build_type)` first, then launches `ninja` via `th08run.bat` (`scripts/build.py:11-37`).
- The natural hook point is the `configure(build_type)` call at `scripts/build.py:12`, because the actual object graph is generated in `configure.py`.
- `--object-name` currently maps directly to `build/objdiff/reimpl/<object>.obj` (`scripts/build.py:88-90`). That behavior should stay unchanged, but the generated `objdiff` object must come from the patched `.obj`, not the raw compiler output.

### `scripts/configure.py`

- Compile rules are emitted first (`cc`, `cc_pbg`, per-source `cc_<name>`), then link rules, then the build graph.
- Regular source objects are currently written directly to `build/<Source>.obj` (`scripts/configure.py:190-202`).
- `build/objdiff/reimpl/<Source>.obj` is generated from `build/<Source>.obj` through `rename_symbols` (`scripts/configure.py:197-202`).
- `th08.exe`, `th08e.dll`, `th08.def`, and `objdiff` all ultimately depend on the same `build/<Source>.obj` list (`scripts/configure.py:273-296`, `313-322`).
- That means the patcher should run on `build/<Source>.obj` before `rename_symbols`, `gendef`, and `link`.

### `scripts/compare_function_bytes.py`

- This script already does the two hardest low-level parts the patcher needs:
  - parses COFF section headers and symbol tables,
  - finds a function's byte range by locating its symbol and the next function symbol in the same section.
- It is pure Python and dependency-free, which makes it a good base to refactor into a shared helper such as `scripts/coff_utils.py`.
- The patcher should extend that logic with relocation parsing and writing, but should reuse the same symbol-boundary approach so the tool agrees with the current byte-diff workflow.

### `reccmp-project.yml`

- The project is pinned to one binary target: `th08.exe`.
- The declared SHA256 should also be copied into the patch config, so the patcher refuses to operate when someone tries to reuse the patch set on the wrong executable revision.

## Survey Of Sub-80% `GameManager` Functions

I inspected `diff_report.json` at the repository root. The same schema also exists at `build/diff_report.json`.

There are 29 `GameManager` entries below 80%. The five most likely anti-tamper-related patch targets are:

| Function | Original address | Match | Why it is a candidate |
| --- | --- | ---: | --- |
| `th08::GameManager::AddLives` | `0x43c641` | `0.745098` | Direct wrapper shape: tamper check, crash path, counter mutation, anti-tamper update |
| `th08::GameManager::AddToBombCount` | `0x439883` | `0.745098` | Same wrapper shape as `AddLives` |
| `th08::GameManager::UpdateAntiTamper` | `0x406e50` | `0.500000` | Tiny anti-tamper support stub; safe fixed-byte patch candidate |
| `th08::GameManager::IsTampered` | `0x40bb80` | `0.428571` | Tiny anti-tamper support stub; exact-byte restoration is straightforward |
| `th08::GameManager::CalcAntiTamperChecksum` | `0x439a2e` | `0.428571` | Tiny checksum stub directly adjacent to the same subsystem |

Notes from source and diff inspection:

- `AddPower` also has the same wrapper structure in source, but it is already `1.0` matched and therefore should not be touched.
- In the current report, `AddLives` and `AddToBombCount` already match the `call IsTampered / test / je / CRASH_GAME` portion. Their remaining mismatch is in the small payload window after the crash path plus the epilogue shape.
- That is actually good news for a binary patcher: the necessary rewrite is narrow, fixed-size, and does not require control-flow surgery.

## Architecture

### Injection point

Inject the patcher **post-`cl`, pre-link**, at the object-file stage.

Recommended build graph for opt-in objects:

```text
src/GameManager.cpp
  -> cc_GameManager
  -> build/raw/GameManager.obj
  -> patch_coff
  -> build/GameManager.obj
  -> rename_symbols
  -> build/objdiff/reimpl/GameManager.obj

build/GameManager.obj
  -> gendef
  -> link th08.exe / th08e.dll
```

### Why here

- Patching after link is too late and makes function targeting harder.
- Patching before compile is exactly the source-level path that is already saturated.
- Patching the `.obj` keeps symbol names, section boundaries, relocations, and per-function isolation.

### `scripts/build.py` hook

The user-facing hook belongs at the `configure(build_type)` call in `scripts/build.py:12`.

Design change:

```python
# conceptual only
def build(build_type, verbose=False, jobs=1, target=None, patch_config=None):
    configure(build_type, patch_config=patch_config)
    ...
```

And in `main()`:

- add `--patch-config config/binary_patches.json`
- pass that path into `build(...)`

This keeps `build.py` responsible for user intent and `configure.py` responsible for graph generation.

### `scripts/configure.py` graph change

For patched sources, stop compiling directly to the final object path. Instead:

1. compile to `build/raw/<Source>.obj`,
2. run `patch_coff` to emit `build/<Source>.obj`,
3. keep every downstream consumer unchanged.

Concrete first phase:

- only wrap `GameManager.obj`
- leave every other object on the current direct path

This minimizes churn and limits risk to the exact file that contains the known stuck functions.

## Disassembler Choice

Choose **Capstone**.

Capstone is the best fit because the patcher only needs reliable x86 32-bit decode, instruction sizes, and offsets for already-existing bytes inside a COFF section. It has mature Python bindings, widespread packaging, and a much lower integration cost than iced-x86 when no instruction re-encoding is required. `iced-x86` is stronger if the tool ever needs full assembler-grade rewriting, but that is unnecessary for fixed-length byte replacement. `distorm3` is effectively stale for this use case, and `zydis-py` has less friction-free Python packaging in typical Windows setups.

## Patch Format

Use a single JSON file checked into the repo, for example `config/binary_patches.json`.

Design goals:

- explicit allowlist per object and per function,
- whole-function hash guard,
- rule-level preimage checks,
- fixed-size replacements only,
- no implicit patch discovery.

### Proposed schema

```json
{
  "format_version": 1,
  "target": {
    "project": "th08",
    "exe": "th08.exe",
    "sha256": "330fbdbf58a710829d65277b4f312cfbb38d5448b3df523e79350b879213d924"
  },
  "objects": {
    "GameManager.obj": {
      "patches": {
        "th08::GameManager::AddLives": [
          {
            "rule_id": "payload-register-order",
            "symbol": "?AddLives@GameManager@th08@@QAEEH@Z",
            "original_address": "0x43c641",
            "expected_function_sha256_before": "42cc63f6d622094ec68f4a10197144398fe2361240694d0ec4c94d97970e52e4",
            "offset": 35,
            "expected_before_hex": "db45088b45fc8b4808d841748b55fc8b4208d95874",
            "replace_with_hex": "8b45fc8b4008db4508d840748b45fc8b4008d95874",
            "expected_before_asm": [
              "fild dword ptr [ebp + 8]",
              "mov eax, dword ptr [ebp - 4]",
              "mov ecx, dword ptr [eax + 8]",
              "fadd dword ptr [ecx + 0x74]",
              "mov edx, dword ptr [ebp - 4]",
              "mov eax, dword ptr [edx + 8]",
              "fstp dword ptr [eax + 0x74]"
            ],
            "replace_with_asm": [
              "mov eax, dword ptr [ebp - 4]",
              "mov eax, dword ptr [eax + 8]",
              "fild dword ptr [ebp + 8]",
              "fadd dword ptr [eax + 0x74]",
              "mov eax, dword ptr [ebp - 4]",
              "mov eax, dword ptr [eax + 8]",
              "fstp dword ptr [eax + 0x74]"
            ],
            "forbid_relocations": true
          }
        ]
      }
    }
  }
}
```

### Why this format

- `patches` is literally a mapping of function name to a list of replacement rules, which matches the required workflow.
- `expected_function_sha256_before` prevents accidental reuse when the compiler output shifts.
- `offset` makes application deterministic, while `expected_before_hex` prevents blind writes.
- Optional assembly strings are review aids only; the tool should trust bytes, not text.

### Worked example rationale

The `AddLives` example above is based on the actual rebuilt `build/GameManager.obj` bytes currently in this tree:

- function length: `71` bytes
- whole-function SHA256 before patch: `42cc63f6d622094ec68f4a10197144398fe2361240694d0ec4c94d97970e52e4`
- replacement window starts at byte offset `35`

It rewrites only the register scheduling and load ordering window. The branch structure, call relocations, function length, and section layout remain unchanged.

## CLI Pseudocode

```python
import argparse
import hashlib
import json
from pathlib import Path

from capstone import Cs, CS_ARCH_X86, CS_MODE_32


def main():
    args = parse_args()
    patch_db = load_patch_config(args.patch_config)
    validate_target_hash(patch_db["target"], args.target_hash)

    input_path = Path(args.input)
    output_path = Path(args.output)
    obj_name = input_path.name

    obj_cfg = patch_db["objects"].get(obj_name)
    if obj_cfg is None:
        if args.dry_run:
            print(f"SKIP {obj_name}: no patch entry")
            return 0
        copy_file(input_path, output_path)
        return 0

    data = bytearray(input_path.read_bytes())
    coff = parse_coff(data)                      # reuse compare_function_bytes logic
    reloc_index = parse_relocations(data, coff) # section -> relocation offsets
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = False

    any_change = False

    for function_name, rules in obj_cfg["patches"].items():
        symbol = resolve_symbol_name(rules)
        fn = find_function_bytes(coff, data, symbol or function_name)
        if fn is None:
            fail(f"{function_name}: symbol not found")

        fn_bytes = bytes(fn.bytes_view)
        fn_hash = sha256_hex(fn_bytes)
        expected_hash = rules[0]["expected_function_sha256_before"]
        if fn_hash != expected_hash:
            fail(
                f"{function_name}: function hash mismatch "
                f"(expected {expected_hash}, got {fn_hash})"
            )

        insns = list(md.disasm(fn_bytes, 0))
        insn_map = build_instruction_offset_map(insns)  # byte offset -> instruction

        print(f"CHECK {function_name} len={len(fn_bytes)} sha256={fn_hash}")

        for rule in rules:
            offset = rule["offset"]
            before = bytes.fromhex(rule["expected_before_hex"])
            after = bytes.fromhex(rule["replace_with_hex"])

            if len(before) != len(after):
                fail(f"{function_name}:{rule['rule_id']}: variable-size patches forbidden")

            start = offset
            end = offset + len(before)
            if end > len(fn_bytes):
                fail(f"{function_name}:{rule['rule_id']}: patch range out of bounds")

            if start not in insn_map or end not in instruction_boundary_set(insns):
                fail(f"{function_name}:{rule['rule_id']}: patch must start/end on instruction boundary")

            current = fn_bytes[start:end]
            if current != before:
                fail(
                    f"{function_name}:{rule['rule_id']}: pre-patch bytes mismatch "
                    f"(expected {before.hex()}, got {current.hex()})"
                )

            if rule.get("forbid_relocations", True):
                if relocation_hits_range(reloc_index, fn.section_idx, fn.start + start, fn.start + end):
                    fail(f"{function_name}:{rule['rule_id']}: patch overlaps relocation entry")

            print_disasm_window("BEFORE", insns, start, end)
            print_hex_diff(before, after)
            print_optional_expected_asm(rule)

            if not args.dry_run:
                write_range(data, fn.start + start, after)
                any_change = True

                # Re-read function bytes after each write so later rules see current state.
                fn_bytes = bytes(data[fn.start:fn.end])
                insns = list(md.disasm(fn_bytes, 0))
                insn_map = build_instruction_offset_map(insns)

        new_hash = sha256_hex(bytes(data[fn.start:fn.end]))
        print(f"PATCHED {function_name} new_sha256={new_hash}")

    if args.dry_run:
        print("DRY RUN ONLY: no file written")
        return 0

    if not any_change:
        copy_file(input_path, output_path)
        print(f"NO CHANGES {obj_name}: copied through unchanged")
        return 0

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(data)
    print(f"WROTE {output_path}")
    return 0


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--patch-config", required=True)
    parser.add_argument("--target-hash", required=False)
    parser.add_argument("--dry-run", action="store_true")
    return parser.parse_args()
```

## Risk Mitigation

The patcher must be safer than hand-editing bytes in a hex editor. The most important protections are:

### 1. Dry-run mode

- `--dry-run` parses the object, resolves symbols, disassembles the patch window, verifies all hashes and byte preimages, and prints the proposed changes.
- It writes nothing.
- This should be the default mode used in CI verification jobs before enabling write mode locally.

### 2. Whole-function hash check before patching

- Every patched function records `expected_function_sha256_before`.
- If the compiler output changes because of toolchain drift or source edits, the patcher must refuse to patch.
- This avoids silently damaging the ~25 already-100% functions or accidentally applying a stale patch to a nearby-but-different byte pattern.

### 3. Explicit allowlist only

- The patcher must never scan for "similar" functions.
- Only functions explicitly listed in the JSON are eligible.
- First rollout should scope the entire system to `GameManager.obj` only.

### 4. Fail loud on pre-patch mismatch

- Each rule must compare the exact current bytes in the target window against `expected_before_hex`.
- If the bytes do not match exactly, abort with a non-zero exit and print the mismatching hex.
- No best-effort patching.

### 5. Fixed-size replacements only

- Never change function length.
- Never insert or delete instructions.
- Never shift section layout or symbol offsets.
- This preserves link-time relocations, downstream symbol boundaries, and `objdiff` comparability.

### 6. Relocation overlap check

- Parse relocation entries for the target section.
- Reject any patch that overlaps a relocation site unless the rule explicitly opts in.
- For the initial wrapper fixes, there is no reason to touch relocation-bearing bytes.

## Recommended Rollout

1. Phase 1: patch only `GameManager.obj`.
2. Phase 1a: start with `AddLives` and `AddToBombCount`, because they are narrow, fixed-size rewrites with obvious intent.
3. Phase 1b: add `UpdateAntiTamper`, `IsTampered`, and `CalcAntiTamperChecksum` only after the tool proves stable on the first two.
4. Phase 2: once the workflow is validated, expand the JSON allowlist to other stubborn wrapper-like functions in other objects.

## Expected Outcome

This design avoids trying to outsmart MSVC. Instead it treats the last mile as a controlled object transformation step, using the existing build graph and the existing COFF-byte inspection approach already present in `scripts/compare_function_bytes.py`.

If implemented carefully, it should let the project recover many narrow last-mile mismatches without destabilizing already-matched functions.
