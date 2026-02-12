use crate::windows::ids::WindowKind;
use crate::windows::manager;

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
    manager::position_window(&app, kind, x, y)
        .map_err(|error| format!("failed to move window: {error}"))
}

#[tauri::command]
pub fn resize_window(app: tauri::AppHandle, kind: WindowKind, width: f64, height: f64) -> Result<(), String> {
    manager::resize_window(&app, kind, width, height)
        .map_err(|error| format!("failed to resize window: {error}"))
}
