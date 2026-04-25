//! DLL-injection loader. Target: launch the user-supplied th0X.exe with
//! `CREATE_SUSPENDED`, allocate a remote string containing the DLL path,
//! stick `LoadLibraryW` into the target via `CreateRemoteThread`, resume
//! the main thread. Classic Windows technique.
//!
//! Bridging to the existing C++ `th08_platform_loader.exe` is the Phase
//! 0/1 fallback — V1 we just shell out to it, V2 we inline this code so
//! the client ships as a single .exe.

use std::path::Path;
use std::time::Duration;

use anyhow::{anyhow, Context, Result};
use sha2::{Digest, Sha256};
use tokio::fs::File;
use tokio::io::AsyncReadExt;

/// Hex-encoded SHA256 of the exe at `path`. Used to verify the user's
/// local copy matches an expected `games.expected_exe_sha256`.
pub async fn verify_sha256(path: &str, expected_hex: &str) -> Result<bool> {
    let mut f = File::open(path).await.with_context(|| format!("open {path}"))?;
    let mut hasher = Sha256::new();
    let mut buf = vec![0u8; 64 * 1024];
    loop {
        let n = f.read(&mut buf).await.context("read")?;
        if n == 0 { break }
        hasher.update(&buf[..n]);
    }
    let got = hex::encode(hasher.finalize());
    Ok(got.eq_ignore_ascii_case(expected_hex))
}

/// Launch the target game and inject the DLL. Returns the process id
/// of the running game.
#[cfg(windows)]
pub async fn launch(game_id: &str, exe_path: &str, dll_path: &str) -> Result<u32> {
    tracing::info!(game_id, exe_path, dll_path, "launching");

    let exe_path = exe_path.to_string();
    let dll_path = dll_path.to_string();
    if !Path::new(&exe_path).exists() {
        return Err(anyhow!("exe not found: {exe_path}"));
    }
    if !Path::new(&dll_path).exists() {
        return Err(anyhow!("dll not found: {dll_path}"));
    }

    // Do the Windows dance on a blocking thread.
    tokio::task::spawn_blocking(move || inject_dll(&exe_path, &dll_path))
        .await
        .context("spawn_blocking")?
}

#[cfg(not(windows))]
pub async fn launch(_game_id: &str, _exe_path: &str, _dll_path: &str) -> Result<u32> {
    Err(anyhow!("launch_game only supported on Windows"))
}

#[cfg(windows)]
fn inject_dll(exe_path: &str, dll_path: &str) -> Result<u32> {
    use std::os::windows::ffi::OsStrExt;
    use std::ffi::OsStr;

    use windows::core::PCWSTR;
    use windows::Win32::Foundation::{CloseHandle, HANDLE};
    use windows::Win32::System::Diagnostics::Debug::WriteProcessMemory;
    use windows::Win32::System::LibraryLoader::{GetModuleHandleW, GetProcAddress};
    use windows::Win32::System::Memory::{
        VirtualAllocEx, VirtualFreeEx, MEM_COMMIT, MEM_RELEASE, MEM_RESERVE, PAGE_READWRITE,
    };
    use windows::Win32::System::Threading::{
        CreateProcessW, CreateRemoteThread, ResumeThread, CREATE_SUSPENDED,
        PROCESS_INFORMATION, STARTUPINFOW, LPTHREAD_START_ROUTINE,
    };

    unsafe {
        let exe_wide = to_wide(exe_path);
        let mut cmd: Vec<u16> = exe_wide.clone();
        let mut si = STARTUPINFOW {
            cb: std::mem::size_of::<STARTUPINFOW>() as u32,
            ..Default::default()
        };
        let mut pi = PROCESS_INFORMATION::default();

        CreateProcessW(
            PCWSTR(exe_wide.as_ptr()),
            windows::core::PWSTR(cmd.as_mut_ptr()),
            None, None, false.into(),
            CREATE_SUSPENDED,
            None, None, &si, &mut pi,
        ).ok().context("CreateProcessW")?;

        // Scope guards — make sure we don't leak thread/process handles
        // even on error paths.
        let pid = pi.dwProcessId;
        let result = (|| -> Result<u32> {
            let dll_wide = to_wide(dll_path);
            let dll_bytes = dll_wide.len() * std::mem::size_of::<u16>();

            let remote_mem = VirtualAllocEx(
                pi.hProcess,
                None,
                dll_bytes,
                MEM_COMMIT | MEM_RESERVE,
                PAGE_READWRITE,
            );
            if remote_mem.is_null() {
                return Err(anyhow!("VirtualAllocEx failed"));
            }
            let mut written = 0usize;
            WriteProcessMemory(
                pi.hProcess,
                remote_mem,
                dll_wide.as_ptr() as *const _,
                dll_bytes,
                Some(&mut written),
            ).ok().context("WriteProcessMemory")?;

            let kernel32 = GetModuleHandleW(windows::core::w!("kernel32.dll"))
                .context("GetModuleHandleW kernel32")?;
            let load_lib_w = GetProcAddress(kernel32, windows::core::s!("LoadLibraryW"))
                .ok_or_else(|| anyhow!("GetProcAddress LoadLibraryW"))?;

            let start_routine: LPTHREAD_START_ROUTINE =
                Some(std::mem::transmute(load_lib_w));
            let thread = CreateRemoteThread(
                pi.hProcess, None, 0,
                start_routine,
                Some(remote_mem),
                0, None,
            ).context("CreateRemoteThread")?;

            // Wait briefly so LoadLibraryW completes before we resume the
            // main thread. A full WaitForSingleObject would block forever
            // if the DLL's DllMain is slow — we just give it a moment and
            // let the DLL log its own readiness.
            std::thread::sleep(Duration::from_millis(100));

            // Resume the game's main thread. It's alive from now on.
            ResumeThread(pi.hThread);

            // Free the remote memory (the DLL path copy); the loaded DLL
            // already copied the string into its own space.
            let _ = VirtualFreeEx(pi.hProcess, remote_mem, 0, MEM_RELEASE);
            let _ = CloseHandle(thread);

            Ok(pid)
        })();

        let _ = CloseHandle(pi.hThread);
        let _ = CloseHandle(pi.hProcess);
        result
    }
}

#[cfg(windows)]
fn to_wide(s: &str) -> Vec<u16> {
    use std::ffi::OsStr;
    use std::os::windows::ffi::OsStrExt;
    OsStr::new(s).encode_wide().chain(std::iter::once(0)).collect()
}

// Silence unused-import warnings when building non-Windows targets (CI).
#[cfg(not(windows))]
#[allow(dead_code)]
fn _noop() {
    let _ = HANDLE_PLACEHOLDER;
}
#[cfg(not(windows))]
#[allow(dead_code)]
const HANDLE_PLACEHOLDER: usize = 0;
