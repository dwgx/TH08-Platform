# TH08-Platform

**Private.** Touhou 8 — *Imperishable Night* — co-op multiplayer platform via DLL injection into the original game binary.

## What this is

Two tracks bundled in one monorepo:

1. **`game/` — th08 decomp** (fork of [GensokyoClub/th08](https://github.com/GensokyoClub/th08), AGPL-3.0). Source-level reconstruction of th08 v1.00d used as a knowledge base for struct layouts and function addresses. Currently ~55% byte-matched.

2. **`dll/` — multiplayer DLL + loader.** Hooks into the original `th08.exe` at runtime via MinHook; adds UDP rollback netcode and a second player. The original binary is never modified on disk — only injected into memory at process start via `th08_platform_loader.exe`.

The platform goal: a lobby/matchmaking service where two players launch, get matched, and the DLL handles synchronized gameplay.

## Status — 2026-04-25

| Area | State |
|---|---|
| Decomp (bytes matching original) | ~55% (399+ of 722 tracked functions) |
| DLL Phase 0 (proof-of-injection hook) | scaffold ready, not yet built |
| DLL Phase 1-5 (state capture, netcode, P2) | planned in `docs/DLL_PLAN.md` |
| Platform / lobby UI | not started |

## Repo layout

```
TH08-Platform/
├── README.md
├── .gitignore                          excludes th08.exe + build artifacts
│
├── game/                               th08 decomp (fork of GensokyoClub/th08)
│   ├── src/                            game source reconstructed in C++
│   ├── config/                         mapping.csv, reccmp symbols, etc.
│   ├── scripts/                        VS2002 + DX8 build system
│   ├── objdiff.json                    objdiff config
│   ├── reccmp-project.yml              reccmp project config
│   └── LICENSE                         AGPL-3.0 (inherited upstream)
│
├── dll/                                multiplayer DLL (new)
│   ├── CMakeLists.txt                  FetchContent pulls MinHook
│   ├── src/
│   │   ├── main.cpp                    DllMain
│   │   ├── logging.{h,cpp}             logs to %LOCALAPPDATA%\th08_platform\log.txt
│   │   └── hooks/game_loop.{h,cpp}     Phase 0: hook GameManager::OnUpdate @ 0x43aa03
│   └── loader/loader.cpp               injector exe
│
├── tools/                              decomp factory helpers
│   ├── ida_helper.py                   IDA MCP HTTP client
│   ├── batch_analyze[_v2..v5].py       pre-analyzers that emit worksheets
│   └── copy_to_monorepo.py             snapshot th08-decomp → this repo
│
└── docs/
    ├── DLL_PLAN.md                     architecture + 6-phase roadmap
    └── worksheets/                     pre-analysis worksheets used by codex batches
```

## Prerequisites

- **`th08.exe` v1.00d** — SHA256 `330fbdbf58a710829d65277b4f312cfbb38d5448b3df523e79350b879213d924`. You supply this; it is **not** in the repo (copyrighted by ZUN / Team Shanghai Alice).
- **Python 3.12+** (for build scripts and factory tools)
- **Git**, **CMake 3.20+**
- **Visual Studio 2022** (for DLL build) — 32-bit target required
- **IDA Pro 9.x** with `ida-pro-mcp` plugin (only needed to run the decomp factory)
- **reccmp** — `pip install reccmp` (only for running diff against original)

## Build — game decomp (the C++ that should match th08.exe byte-for-byte)

```bash
cd game
cp /your/th08.exe resources/th08.exe
python scripts/create_devenv.py scripts/dls scripts/prefix   # first time only, ~10 min
python scripts/build.py                                       # produces build/th08.exe
```

Verify how close we are to byte-matching:

```bash
cd build
reccmp-reccmp --target th08 --json ../diff_report.json
# check % per-function in diff_report.json
```

## Build — DLL (Phase 0 hello-world)

```bash
cd dll
cmake -B build -A Win32
cmake --build build --config Release
# outputs: build/bin/th08_platform.dll and th08_platform_loader.exe
```

## Run — inject DLL into your own th08.exe

```bash
dll/build/bin/th08_platform_loader.exe /path/to/your/th08.exe
# then tail %LOCALAPPDATA%\th08_platform\log.txt to see frame counter ticking
```

## Roadmap

Six phases detailed in `docs/DLL_PLAN.md`. Summary:

- **Phase 0** — hello DLL, hooks GameManager::OnUpdate, logs frames *(current scaffold)*
- **Phase 1** — state capture / restore (rollback foundation)
- **Phase 2** — input hook (merge local + remote)
- **Phase 3** — UDP lockstep netcode
- **Phase 4** — rollback netcode (~Giuroll level)
- **Phase 5** — second player ship + shared HUD
- **Phase 6** — external lobby / matchmaking service

## Credits

- [GensokyoClub/th06](https://github.com/happyhavoc/th06) + [GensokyoClub/th08](https://github.com/GensokyoClub/th08) — the decomp foundation
- [Giuroll](https://github.com/Giufin/giuroll) — rollback netcode reference (Soku / th12.3)
- [th06_multi_net](https://github.com/RUEEE/th06_multi_net) — proof that multiplayer via decomp is achievable
- [reccmp](https://github.com/isledecomp/reccmp), [MinHook](https://github.com/TsudaKageyu/minhook), [Dear ImGui](https://github.com/ocornut/imgui) — the tooling we lean on

## License

AGPL-3.0-or-later, inherited from `game/` upstream. Private repo: no distribution of modified game binaries. Users must supply their own `th08.exe`.
