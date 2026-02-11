use crate::windows::ids::WindowKind;
use crate::windows::manager;

#[tauri::command]
pub fn open_window(app: tauri::AppHandle, kind: WindowKind) -> Result<(), String> {
    manager::show_window(&app, kind).map_err(|error| format!("failed to open window: {error}"))
}

#[tauri::command]
pub fn close_window(app: tauri::AppHandle, kind: WindowKind) -> Result<(), String> {
    manager::hide_window(&app, kind).map_err(|error| format!("failed to close window: {error}"))
}

#[tauri::command]
pub fn move_window(app: tauri::AppHandle, kind: WindowKind, x: f64, y: f64) -> Result<(), String> {
    manager::position_window(&app, kind, x, y)
        .map_err(|error| format!("failed to move window: {error}"))
}
