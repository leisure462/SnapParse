use std::path::PathBuf;

use tauri::{Emitter, Manager};
use tauri_plugin_autostart::ManagerExt;

use crate::ocr;
use crate::settings::model::AppSettings;
use crate::settings::store;

fn resolve_config_root(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    app.path()
        .app_config_dir()
        .map_err(|error| format!("failed to resolve app config dir: {error}"))
}

fn sync_autostart_state(app: &tauri::AppHandle, enabled: bool) -> Result<(), String> {
    let autolaunch = app.autolaunch();
    let current = autolaunch
        .is_enabled()
        .map_err(|error| format!("failed to query auto-launch state: {error}"))?;

    if current == enabled {
        return Ok(());
    }

    if enabled {
        autolaunch
            .enable()
            .map_err(|error| format!("failed to enable auto-launch: {error}"))
    } else {
        autolaunch
            .disable()
            .map_err(|error| format!("failed to disable auto-launch: {error}"))
    }
}

#[tauri::command]
pub fn get_settings(app: tauri::AppHandle) -> Result<AppSettings, String> {
    let config_root = resolve_config_root(&app)?;
    store::load_settings(&config_root).map_err(|error| error.to_string())
}

#[tauri::command]
pub fn save_settings(app: tauri::AppHandle, settings: AppSettings) -> Result<AppSettings, String> {
    let config_root = resolve_config_root(&app)?;
    store::save_settings(&config_root, &settings).map_err(|error| error.to_string())?;
    sync_autostart_state(&app, settings.general.launch_at_startup)?;
    ocr::sync_ocr_hotkey(&app, &settings)?;

    // Notify all windows that settings have changed so they can re-apply
    // font size, theme, and other runtime-configurable values.
    let _ = app.emit("settings-changed", &settings);

    Ok(settings)
}

#[tauri::command]
pub fn reset_settings(app: tauri::AppHandle) -> Result<AppSettings, String> {
    let config_root = resolve_config_root(&app)?;
    let defaults = store::reset_settings(&config_root).map_err(|error| error.to_string())?;
    sync_autostart_state(&app, defaults.general.launch_at_startup)?;
    ocr::sync_ocr_hotkey(&app, &defaults)?;

    let _ = app.emit("settings-changed", &defaults);

    Ok(defaults)
}
