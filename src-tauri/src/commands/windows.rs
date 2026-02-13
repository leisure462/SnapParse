use crate::windows::ids::WindowKind;
use crate::windows::manager;
use std::sync::{Mutex, OnceLock};

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct PendingOptimizeRequest {
    pub text: String,
    pub target: Option<String>,
    pub title: Option<String>,
    pub custom_prompt: Option<String>,
    pub custom_model: Option<String>,
    pub request_id: Option<u64>,
}

fn pending_optimize_request_store() -> &'static Mutex<Option<PendingOptimizeRequest>> {
    static STORE: OnceLock<Mutex<Option<PendingOptimizeRequest>>> = OnceLock::new();
    STORE.get_or_init(|| Mutex::new(None))
}

#[tauri::command]
pub fn open_window(app: tauri::AppHandle, kind: WindowKind) -> Result<(), String> {
    eprintln!("[cmd] open_window called: {:?}", kind);
    manager::show_window(&app, kind).map_err(|error| {
        let msg = format!("failed to open window {:?}: {error}", kind);
        eprintln!("[cmd] {}", msg);
        msg
    })
}

#[tauri::command]
pub fn close_window(app: tauri::AppHandle, kind: WindowKind) -> Result<(), String> {
    manager::hide_window(&app, kind).map_err(|error| format!("failed to close window: {error}"))
}

#[tauri::command]
pub fn move_window(app: tauri::AppHandle, kind: WindowKind, x: f64, y: f64) -> Result<(), String> {
    eprintln!("[cmd] move_window called: {:?} to ({}, {})", kind, x, y);
    manager::position_window_logical(&app, kind, x, y)
        .map_err(|error| format!("failed to move window: {error}"))
}

#[tauri::command]
pub fn resize_window(app: tauri::AppHandle, kind: WindowKind, width: f64, height: f64) -> Result<(), String> {
    manager::resize_window(&app, kind, width, height)
        .map_err(|error| format!("failed to resize window: {error}"))
}

#[tauri::command]
pub fn open_external_url(url: String) -> Result<(), String> {
    let target = url.trim();
    if !(target.starts_with("http://") || target.starts_with("https://")) {
        return Err(String::from("url must start with http:// or https://"));
    }

    #[cfg(target_os = "windows")]
    {
        std::process::Command::new("rundll32")
            .arg("url.dll,FileProtocolHandler")
            .arg(target)
            .spawn()
            .map_err(|error| format!("failed to open url: {error}"))?;
        return Ok(());
    }

    #[cfg(target_os = "macos")]
    {
        std::process::Command::new("open")
            .arg(target)
            .spawn()
            .map_err(|error| format!("failed to open url: {error}"))?;
        return Ok(());
    }

    #[cfg(all(unix, not(target_os = "macos")))]
    {
        std::process::Command::new("xdg-open")
            .arg(target)
            .spawn()
            .map_err(|error| format!("failed to open url: {error}"))?;
        return Ok(());
    }

    #[allow(unreachable_code)]
    Err(String::from("unsupported platform for opening external url"))
}

#[tauri::command]
pub fn set_pending_optimize_request(payload: PendingOptimizeRequest) -> Result<(), String> {
    if payload.text.trim().is_empty() {
        return Err(String::from("pending optimize request text must not be empty"));
    }

    let store = pending_optimize_request_store();
    let mut guard = store
        .lock()
        .map_err(|error| format!("failed to lock optimize request store: {error}"))?;
    *guard = Some(payload);
    Ok(())
}

#[tauri::command]
pub fn take_pending_optimize_request() -> Result<Option<PendingOptimizeRequest>, String> {
    let store = pending_optimize_request_store();
    let mut guard = store
        .lock()
        .map_err(|error| format!("failed to lock optimize request store: {error}"))?;
    Ok(guard.take())
}
