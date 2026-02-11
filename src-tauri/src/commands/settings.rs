use std::path::PathBuf;

use tauri::Manager;

use crate::settings::model::AppSettings;
use crate::settings::store;

fn resolve_config_root(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    app.path()
        .app_config_dir()
        .map_err(|error| format!("failed to resolve app config dir: {error}"))
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
    Ok(settings)
}

#[tauri::command]
pub fn reset_settings(app: tauri::AppHandle) -> Result<AppSettings, String> {
    let config_root = resolve_config_root(&app)?;
    store::reset_settings(&config_root).map_err(|error| error.to_string())
}
