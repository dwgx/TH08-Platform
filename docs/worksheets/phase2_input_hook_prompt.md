# Phase 2 — Input Hook (Codex Worker Prompt — DRAFT, dispatch after Phase 1 verifies)

**Project**: th08_platform DLL multiplayer mod
**Working dir**: `D:\Project\THGlobal\TH08-Platform\dll`
**Reference (read-only)**: `D:\Project\THGlobal\th08-decomp\config\mapping.csv`
**Karpathy applies**: surgical changes, simplicity, verifiable goals.

---

## Goal

Hook `th08::Controller::GetInput` so the DLL can observe (and later override) the per-frame input bitfield. Phase 2 ends when:

1. Build succeeds.
2. On next inject test: log shows the input bitfield value every 60 frames (or on edge change). Format: `input: cur=0x%04x (held=%u)`.
3. The hook does NOT modify behavior — just observes. Override is Phase 3+.

---

## Context

### Target function (from `th08-decomp/config/mapping.csv`):

```
th08::Controller::GetInput,0x43d970,0x7ed,unknown,,u8
```

- Address: `0x0043d970` (image base 0x400000)
- Size: 0x7ed bytes (2029 — big function, processes DirectInput state into u16 mask)
- Calling convention: marked `unknown`, but ALL other Controller methods in mapping.csv with known conv are `__fastcall` (`GetJoystickCaps`, `SetButtonFromDirectInputJoystate`, `SetButtonFromControllerInputs`, `ResetKeyboard`). **Use `__fastcall` for the hook**.
- Return: `u8` (or u32; the bitfield gets written to `g_CurFrameInput @ 0x0164d528` which is u16, so the function returns u8 status, and writes input to global).
- Args: not listed in mapping.csv. Function is likely `__fastcall(void)` since it reads from a global Controller instance and writes to global input vars. Verify by inspecting via:
  ```
  python ../../tools/ida_helper.py call decompile '{"addr":"0x43d970"}'
  ```
  before declaring the typedef.

### Globals (already in `state/globals.h`):

- `g_CurFrameInput @ 0x0164d528` (u16) — the bitfield Controller::GetInput populates
- `g_LastFrameInput @ 0x0164d530` (u16) — previous frame's value
- `g_NumOfFramesInputsWereHeld @ 0x0164d53c` (u32) — held-key counter

### Existing scaffolding:

- `src/hooks/game_loop.cpp` already has the OnUpdate hook installed via MinHook
- `src/state/globals.h` already declares all input addresses
- `src/state/frame_state.cpp` already captures input globals every frame

---

## File layout

Create:
```
src/hooks/input.h      bool install_input_hook(); void uninstall_input_hook();
src/hooks/input.cpp    MinHook on Controller::GetInput
```

Modify:
- `src/main.cpp` — call `install_input_hook()` after `install_game_loop_hook()` in init thread
- `CMakeLists.txt` — add `src/hooks/input.cpp` to `add_library(th08_platform SHARED ...)`

Do NOT modify:
- Anything in `state/`
- `loader/`
- `logging.{h,cpp}`

---

## Implementation sketch

```cpp
// input.cpp
#include "input.h"
#include "../logging.h"
#include "../state/globals.h"

#include <windows.h>
#include <MinHook.h>
#include <atomic>
#include <cstdint>

namespace th08_platform::hooks {

namespace {
constexpr std::uintptr_t kControllerGetInputRVA = 0x43d970 - 0x400000;

// VERIFY this signature via IDA hex-rays before relying on it:
using GetInput_t = std::uint8_t(__fastcall*)();
GetInput_t g_original_GetInput = nullptr;

std::atomic<std::uint64_t> g_input_call_count{0};
std::uint16_t g_last_logged_input = 0xffff;

std::uint8_t __fastcall hooked_GetInput()
{
    const auto ret = g_original_GetInput();

    const auto cur = *globals::at<std::uint16_t>(globals::kAddr_g_CurFrameInput);
    const auto held = *globals::at<std::uint32_t>(globals::kAddr_g_NumOfFramesInputsWereHeld);
    const auto n = g_input_call_count.fetch_add(1, std::memory_order_relaxed) + 1;

    // Log every 60 calls AND whenever the input bitfield changes
    if (n % 60 == 0 || cur != g_last_logged_input) {
        th08_platform::log_line("input: cur=0x%04x (held=%u, call=%llu)",
            cur, held, static_cast<unsigned long long>(n));
        g_last_logged_input = cur;
    }

    return ret;
}

void* resolve_target() {
    HMODULE base = GetModuleHandleA(nullptr);
    if (!base) return nullptr;
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uint8_t*>(base) + kControllerGetInputRVA);
}
}  // namespace

bool install_input_hook()
{
    void* target = resolve_target();
    if (!target) {
        th08_platform::log_line("input hook: resolve_target null");
        return false;
    }
    th08_platform::log_line("hooking Controller::GetInput at %p", target);

    if (MH_CreateHook(target, reinterpret_cast<void*>(&hooked_GetInput),
                      reinterpret_cast<LPVOID*>(&g_original_GetInput)) != MH_OK) {
        th08_platform::log_line("input hook: MH_CreateHook failed");
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        th08_platform::log_line("input hook: MH_EnableHook failed");
        return false;
    }
    th08_platform::log_line("input hook installed");
    return true;
}

void uninstall_input_hook()
{
    // MinHook's MH_DisableHook(MH_ALL_HOOKS) in main.cpp handles it.
}

}  // namespace th08_platform::hooks
```

**CRITICAL**: before writing, fetch the actual hex-rays for `0x43d970` to verify:
- Does it take args (refining the typedef)?
- Does it write to `g_CurFrameInput` (or some other global)?
- Calling convention sanity check.

If hex-rays disagrees with our assumptions, adjust the typedef. Don't ship guesswork.

---

## Build + verify

1. `cmake --build build --config Release` → no errors, no new warnings (beyond C4819).
2. `dumpbin /HEADERS build/bin/Release/th08_platform.dll` should still say `i386`.
3. DO NOT launch th08.exe. The user will run inject test after Phase 1 is verified separately.

---

## Return format

```markdown
## Phase 2 — done

### Files created
### Files modified
### Hook details
- function signature you settled on (after verifying hex-rays)
- log format example
### Build result
### What user will see on next inject (log lines)
### Open questions
```
