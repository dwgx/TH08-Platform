# Phase 6a — th08.exe single-instance mutex finding

> Auditor: Claude (Opus 4.7) via IDA-MCP, 2026-04-26.
> Scope: TH08 IN v1.00d (image base 0x400000, SHA prefix 330fbdbf).

## Verdict

**Yes, th08.exe enforces single-instance via a named mutex.** Two `th08.exe`
processes cannot coexist on the same OS session without intervention.
Mutex name is `"Touhou 08 App"` and it lives in code, not config.

## Evidence

### Imports
- `KERNEL32!CreateMutexA` is imported and only called from one site.
- `KERNEL32!CreateMutexW` is **not** imported.
- `USER32!FindWindowA/W` are **not** imported.
- `KERNEL32!CreateFileA` is imported (used for resources, not lock-file).

### IAT entry
| Symbol | IAT addr |
|---|---|
| `CreateMutexA` | `0x4b4098` |
| `CreateEventA` | `0x4b40c4` (used by sound subsystem, not mutex check) |

### Single call site
`sub_443420` @ `0x443420` (called from `sub_4418C0` @ `0x441951`):

```c
int sub_443420()
{
    dword_17CE6FC = (int)CreateMutexA(0, 1, "Touhou 08 App");  // 0x44343f
    if (GetLastError() == 183 /* ERROR_ALREADY_EXISTS */)      // 0x44344f
    {
        sub_43EB60(...);   // shows error message via &byte_4B64C8
        return -1;         // signals startup failure → th08 exits
    }
    /* ... unrelated startup path validation (filename/StartupInfo) ... */
    return dword_17CE6FC ? 0 : -1;
}
```

### Why the second instance dies
- First `CreateMutexA(NULL, TRUE, "Touhou 08 App")` succeeds, owner = first proc.
- Second `CreateMutexA(NULL, TRUE, "Touhou 08 App")` returns a handle to the
  existing kernel object AND sets last-error to `ERROR_ALREADY_EXISTS (183)`.
- ZUN explicitly checks `GetLastError() == 183` and returns -1 → exit.

## Workaround options considered

| Option | Verdict |
|---|---|
| (A) Two separate th08 install dirs | **Rejected** — mutex name is OS-session global, not bound to install path. Both installs collide on `"Touhou 08 App"`. |
| (B) Loader copies th08.exe + *.dat to `%TEMP%\inst_<pid>\` per launch | **Rejected** — same as A, mutex name is hardcoded in the binary string, copying does not change it. |
| (C) DLL patches CreateMutexA at runtime to uniqufy the name | **Chosen** — small, contained, no binary-on-disk modification, no game-files duplication. |

## Chosen implementation: IAT hook on CreateMutexA

**Module**: `dll/src/hooks/instance_unmutex.{h,cpp}` (new, Phase 6a).

**Strategy**:
1. In `DllMain` `DLL_PROCESS_ATTACH`, before any other init, swap th08's
   IAT slot at `0x4b4098` with a pointer to our `hook_CreateMutexA`. The
   write happens while th08's main thread is still suspended (loader uses
   `CREATE_SUSPENDED` + `CreateRemoteThread(LoadLibraryA)` + `WaitForSingleObject`
   + `ResumeThread`), so race-free.
2. `hook_CreateMutexA(attrs, initial, name)`:
   - If `name` is exactly `"Touhou 08 App"` → forward to real `CreateMutexA`
     with `"th08_platform_inst_<PID>"` instead. Each loader-launched th08
     gets its own unique mutex; `ERROR_ALREADY_EXISTS` never fires.
   - Otherwise → forward unchanged. We don't disturb future ZUN-named
     mutex callers (none exist in v1.00d but be defensive).

**Memory write detail**:
- `VirtualProtect(iat_slot, sizeof(void*), PAGE_READWRITE, &old)` first —
  the IAT lives in `.idata` which is normally read-only.
- Replace pointer.
- Restore protection with the original flags.

**No MinHook / detours required** — IAT hook is a single-DWORD pointer
swap, simpler than inline hooking and doesn't need a trampoline.

## Side effects / non-issues

- **Single-instance UX broken intentionally.** A user double-clicking
  the loader twice now gets two TH08 instances instead of an error
  dialog. This is the desired behavior for Phase 6 multiplayer. If we
  ever want to restore single-instance behavior for solo play, gate the
  patch on a loader flag (e.g., `--multi-instance`).
- **Patch only affects loader-launched th08.** Direct `th08.exe`
  double-click without our DLL is unchanged; ZUN's check still fires.
- **Mutex is process-bound.** The OS releases our patched mutex on
  process exit, no leak.
- **Hardcoded address `0x4b4098`** is th08 v1.00d-specific. If we later
  support other game versions, lookup the IAT slot dynamically via
  `GetProcAddress` against the loaded `kernel32.dll` mapped into th08's
  address space; for v1.00d the static address is fine.

## Verification path (post-build)

1. Build DLL + loader with new module included.
2. Run `loader.exe --host --listen 7480 D:/Project/THGlobal/th08/th08.exe`.
3. Run `loader.exe --peer 127.0.0.1:7480 --listen 7481 D:/Project/THGlobal/th08/th08.exe`.
4. Both windows should appear at the title screen. Without our patch, the
   second one would silently exit.
5. Check `%LOCALAPPDATA%\th08_platform\log.txt` for any IAT-patch
   diagnostic line we add.

(Defer actual launch to user — per `feedback_no_autolaunch_th08.md`,
Claude does not auto-launch th08.exe.)
