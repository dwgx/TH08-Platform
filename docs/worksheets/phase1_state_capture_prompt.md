# Phase 1 — State Capture / Restore (Codex Worker Prompt)

**Project**: th08_platform DLL multiplayer mod
**Working dir**: `D:\Project\THGlobal\TH08-Platform\dll`
**Reference (read-only)**: `D:\Project\THGlobal\th08-decomp\src\*.hpp` and `D:\Project\THGlobal\th08-decomp\config\mapping.csv`
**Karpathy guideline applies**: think before coding, simplicity first, surgical changes, verifiable goals. Do NOT add abstractions beyond what is asked. Do NOT speculate on future features.

---

## Goal

Add deterministic per-frame state capture + restore to `th08_platform.dll`. Foundation for rollback netcode (Phase 4). Phase 1 ends when:

1. `cmake --build build --config Release` succeeds inside `dll/`.
2. New code paths compile under MSVC (Visual Studio 2022, Win32, /std:c++20).
3. The hooked frame loop calls `capture(state)` per frame and logs the captured payload size every 60 frames in addition to the existing frame counter.
4. `restore(state)` exists and round-trips data back to the same addresses (verified by a unit-style debug helper, no game launch).

Do NOT implement: actual rollback / re-simulation, network code, ImGui overlay, second player. Those are Phase 2-5.

---

## Existing scaffolding

| File | Role |
|---|---|
| `src/main.cpp` | DllMain → init thread → installs hook |
| `src/logging.{h,cpp}` | log_init / log_line / log_shutdown to `%LOCALAPPDATA%\th08_platform\log.txt` |
| `src/hooks/game_loop.{h,cpp}` | MinHook on `th08::GameManager::OnUpdate` at `0x00439bc7`, `__fastcall(GameManager*)`. Already verified working: 60 FPS ticks logged for 1860+ frames. |
| `loader/loader.cpp` | CreateProcess+CreateRemoteThread injector |
| `CMakeLists.txt` | C/CXX, Win32, FetchContent → MinHook v1.3.3, defines `minhook` STATIC lib explicitly because upstream has no CMakeLists |

Build is x86 Win32 because `th08.exe` is 32-bit. **Do not change architecture.**

---

## Targets — what to capture per frame

All addresses are absolute virtual addresses for `th08.exe v1.00d`, image base `0x00400000`. Sources: `th08-decomp/config/mapping.csv` (line numbers given) and `th08-decomp/src/*.hpp` (`C_ASSERT(sizeof(...) == ...)`).

| Symbol | Address | Size | Source for size |
|---|---|---|---|
| `g_GameManager` | `0x0160f508` | `0x3de3c` | `GameManager.hpp:320` |
| `*g_GameManager.globals` (ZunGlobals) | dynamic, dereference at runtime | `0xe4` | `Global.hpp:351` |
| `g_Rng` | `0x0164d520` | TBD — see `Rng.hpp` (small struct, ≤16 bytes; verify) | `th08-decomp/src/Rng.hpp` |
| `g_CurFrameInput` | `0x0164d528` | 2 (u16) | `Global.hpp` |
| `g_LastFrameInput` | `0x0164d530` | 2 (u16) | `Global.hpp` |
| `g_IsEighthFrameOfHeldInput` | `0x0164d538` | 4 (u32) | `Global.hpp` |
| `g_NumOfFramesInputsWereHeld` | `0x0164d53c` | 4 (u32) | `Global.hpp` |
| `g_Chain` | `0x0164f548` | TBD — find `ChainList` struct in `Chain.hpp` | `th08-decomp/src/Chain.hpp` |
| `g_ItemManager` | `0x01653648` | `0x17b094` | `ItemManager.hpp:86` |
| `g_EnemyManager` | `0x00577f20` | TBD | `th08-decomp/src/EnemyManager.hpp` |
| `g_BulletManager` | `0x00f54e90` | TBD | `th08-decomp/src/BulletManager.hpp` |
| `g_Background` | `0x004e4030` | `0x6600` | `Background.hpp:39` |
| `g_Supervisor` | `0x017ce758` | `0x364` | `Supervisor.hpp:306` |
| `g_AsciiManager` | `0x004cce20` | `0x171b0` | `AsciiManager.hpp:160` (probably skippable from Phase 1 — text overlay state) |
| `g_ControllerData` | `0x016535a0` | TBD | look up Controller class in `Controller.hpp` |

**Skip for Phase 1 (deferred to later phase or out of scope):**
- `g_AnmManager` (0x18bdc90, size 0x2a2570) — texture / D3D resources, mostly determinism-irrelevant
- `g_SoundPlayer` (0x18b8a68) — audio
- `g_MusicRoom` / `g_Title` / `g_ResultsScreen` — wrong game state (not in-stage)

If you can't find a struct's `sizeof` C_ASSERT, **mark it TBD in code with a comment, set the buffer size to 0, and log a warning**. Do NOT guess. Phase 1 will be incomplete for those regions — that's better than corrupting memory.

---

## File layout to create

```
dll/src/state/
├── globals.h            constexpr uintptr_t addresses + extern struct accessors
├── frame_state.h        struct FrameState
├── frame_state.cpp      capture() / restore() / size accounting
├── heap_journal.h       class HeapJournal (STUB — Phase 4 fills body)
└── heap_journal.cpp     stub bodies that log only
```

Modify:
```
dll/src/hooks/game_loop.cpp   — wire capture() into hooked_OnUpdate
dll/CMakeLists.txt            — add state/*.cpp to add_library(th08_platform ...)
```

Do NOT modify:
```
dll/src/main.cpp
dll/src/logging.{h,cpp}
dll/loader/loader.cpp
```

---

## Detailed design

### `globals.h`

```cpp
#pragma once
#include <cstdint>

namespace th08_platform::globals {

inline constexpr std::uintptr_t kAddr_g_GameManager   = 0x0160f508;
inline constexpr std::uintptr_t kAddr_g_Rng           = 0x0164d520;
inline constexpr std::uintptr_t kAddr_g_CurFrameInput = 0x0164d528;
// ... rest of addresses

inline constexpr std::size_t kSize_GameManager = 0x3de3c;
inline constexpr std::size_t kSize_ZunGlobals  = 0x00e4;
// ... rest of sizes (TBD = 0 with comment)

template <typename T = void>
[[nodiscard]] T* at(std::uintptr_t addr) noexcept {
    return reinterpret_cast<T*>(addr);
}

}  // namespace th08_platform::globals
```

### `frame_state.h`

```cpp
#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace th08_platform::state {

struct CapturedRegion {
    std::uintptr_t address;        // where it lives in th08.exe
    std::size_t size;              // bytes
    std::vector<std::uint8_t> bytes; // captured contents
    const char* label;             // for logging
};

struct FrameState {
    std::uint64_t frame_number = 0;
    std::vector<CapturedRegion> regions;
    std::array<std::uint8_t, 108> fpu{};  // FNSAVE/FRSTOR layout
};

// Capture all tracked regions from live memory into `state`.
// `frame_number` should be supplied by the caller (the hook's frame counter).
void capture(FrameState& state, std::uint64_t frame_number);

// Restore captured regions back to live memory and reload FPU state.
// No-op if `state` is empty.
void restore(const FrameState& state);

// Total bytes summed across all regions in `state`. For logging.
std::size_t total_payload_bytes(const FrameState& state) noexcept;

}  // namespace th08_platform::state
```

### `frame_state.cpp`

Implement `capture` by iterating a static const array of `{address, size, label}` triples (one entry per target above). For dynamic-pointer regions (like `*g_GameManager.globals` → `ZunGlobals`), dereference at capture time:

```cpp
void* gm_globals_ptr = *reinterpret_cast<void**>(globals::kAddr_g_GameManager + offsetof_globals);
```

Look up the actual `globals` member offset from `GameManager.hpp` (it's a member at a known offset; read the struct definition).

For FPU: use inline assembly (MSVC: `__asm fnsave [s.fpu]` then `__asm fwait` to be safe). Or use `_fnsave` / `_frstor` intrinsics if MSVC supplies them. Use whichever compiles cleanly under /std:c++20.

For `restore`: the reverse — `frstor` first (since memcpy of GameManager may use FPU), then memcpy regions back.

`total_payload_bytes`: sum `r.size` for all `r` in `state.regions`.

### `heap_journal.h`

Stub. Just declare the class:
```cpp
class HeapJournal {
public:
    void begin_frame(std::uint64_t frame_number) noexcept;
    void record_alloc(void* ptr, std::size_t size) noexcept;
    void record_free(void* ptr) noexcept;
    void clear() noexcept;
};
```

### `heap_journal.cpp`

Empty bodies that just `log_line("heap_journal: <method>(...) — stub")` for visibility.

### `game_loop.cpp` integration

Add (at the top of `hooked_OnUpdate`, after the `g_frame_count.fetch_add`):

```cpp
static th08_platform::state::FrameState g_current_frame_state;
th08_platform::state::capture(g_current_frame_state, f);
if (f % 60 == 0) {
    th08_platform::log_line("captured: %llu bytes across %zu regions",
        static_cast<unsigned long long>(
            th08_platform::state::total_payload_bytes(g_current_frame_state)),
        g_current_frame_state.regions.size());
}
```

This means Phase 1's user-visible behavior on the next inject test will be:
```
[hh:mm:ss] GameManager::OnUpdate tick: frame 60
[hh:mm:ss] captured: <N> bytes across <M> regions
```

### CMakeLists.txt

Add the `state/*.cpp` to `add_library(th08_platform SHARED ...)`. Three new files: `state/frame_state.cpp`, `state/heap_journal.cpp`. (The `.h` files don't need listing.) No new include dirs, no new dependencies.

---

## Static assertions

Add to `frame_state.cpp` (compile-time guards against drift in the decomp's struct definitions):

```cpp
// If these fail to compile, the decomp's struct sizes drifted from what
// the binary actually has — investigate before changing them.
static_assert(globals::kSize_GameManager == 0x3de3c);
static_assert(globals::kSize_ZunGlobals  == 0x000e4);
// ... etc for known-size targets
```

---

## Build + verify checklist

1. `cd D:\Project\THGlobal\TH08-Platform\dll && cmake --build build --config Release`
2. Build must produce `build/bin/Release/th08_platform.dll` with no compile errors and no new warnings beyond the existing C4819 (Unicode-in-source) ones.
3. Run a static check: `dumpbin /HEADERS build/bin/Release/th08_platform.dll | grep "machine"` should still say `i386` (32-bit). Ignore if dumpbin not on PATH; just confirm cmake ran with `-A Win32`.

DO NOT run th08.exe. DO NOT run the loader. The user has explicitly said no auto-launch.

---

## Return format

Reply with exactly this structure:

```markdown
## Phase 1 — done

### Files created
- list of created files with line counts

### Files modified
- list of modified files

### Build result
- success/failure summary, key diagnostics if any

### Captured regions
- bullet list of {label, address, size}, including TBD ones with size=0

### What the user will see on next inject
- exact log lines (one example per cadence)

### Open questions
- if any sizes were TBD, list which structs need investigation
```

No prose beyond that template. The user is the orchestrator (Claude); be terse.
