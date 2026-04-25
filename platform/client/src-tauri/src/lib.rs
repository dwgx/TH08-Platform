//! TH-Platform desktop client — Tauri core.
//!
//! Responsibilities of this crate (the Rust half of the app):
//!   * Own the window / lifecycle
//!   * Expose `launch_game` / `verify_exe` / `open_external` commands
//!     to the React frontend
//!   * Manage DLL injection (th08_platform.dll → running th08.exe)
//!
//! The React bundle (in ../dist) handles everything user-facing.

mod loader;

use tauri::Manager;

#[tauri::command]
fn app_info() -> serde_json::Value {
    serde_json::json!({
        "name": "TH-Platform",
        "version": env!("CARGO_PKG_VERSION"),
        "os": std::env::consts::OS,
        "arch": std::env::consts::ARCH,
    })
}

#[tauri::command]
async fn verify_exe(path: String, expected_sha256: String) -> Result<bool, String> {
    loader::verify_sha256(&path, &expected_sha256)
        .await
        .map_err(|e| e.to_string())
}

#[tauri::command]
async fn launch_game(
    game_id: String,
    exe_path: String,
    dll_path: String,
) -> Result<u32, String> {
    loader::launch(&game_id, &exe_path, &dll_path)
        .await
        .map_err(|e| e.to_string())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| "th_platform_client=debug,tauri=info".into()),
        )
        .init();

    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_fs::init())
        .plugin(tauri_plugin_os::init())
        .plugin(tauri_plugin_process::init())
        .plugin(tauri_plugin_store::Builder::default().build())
        .setup(|app| {
            if cfg!(debug_assertions) {
                if let Some(w) = app.get_webview_window("main") {
                    w.open_devtools();
                }
            }
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            app_info,
            verify_exe,
            launch_game,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
