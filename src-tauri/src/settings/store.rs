use std::fs;
use std::path::{Path, PathBuf};
use std::sync::{Mutex, OnceLock};

use crate::settings::model::AppSettings;

const APP_SETTINGS_DIR: &str = "SnapParse";
const SETTINGS_FILE_NAME: &str = "settings.json";

#[derive(Debug, thiserror::Error)]
pub enum SettingsStoreError {
    #[error("failed to lock settings store")]
    LockPoisoned,
    #[error("failed to read/write settings file: {0}")]
    Io(#[from] std::io::Error),
    #[error("failed to serialize or deserialize settings: {0}")]
    Serde(#[from] serde_json::Error),
}

fn settings_mutex() -> &'static Mutex<()> {
    static SETTINGS_MUTEX: OnceLock<Mutex<()>> = OnceLock::new();
    SETTINGS_MUTEX.get_or_init(|| Mutex::new(()))
}

fn app_settings_dir(base_dir: &Path) -> PathBuf {
    base_dir.join(APP_SETTINGS_DIR)
}

fn settings_file_path(base_dir: &Path) -> PathBuf {
    app_settings_dir(base_dir).join(SETTINGS_FILE_NAME)
}

pub fn load_settings(base_dir: &Path) -> Result<AppSettings, SettingsStoreError> {
    let _guard = settings_mutex()
        .lock()
        .map_err(|_| SettingsStoreError::LockPoisoned)?;

    let settings_path = settings_file_path(base_dir);

    if !settings_path.exists() {
        return Ok(AppSettings::default());
    }

    let content = fs::read_to_string(settings_path)?;
    let settings = serde_json::from_str::<AppSettings>(&content)?;
    Ok(settings)
}

pub fn save_settings(base_dir: &Path, settings: &AppSettings) -> Result<(), SettingsStoreError> {
    let _guard = settings_mutex()
        .lock()
        .map_err(|_| SettingsStoreError::LockPoisoned)?;

    let settings_dir = app_settings_dir(base_dir);
    fs::create_dir_all(&settings_dir)?;

    let serialized = serde_json::to_string_pretty(settings)?;
    fs::write(settings_file_path(base_dir), serialized)?;

    Ok(())
}

pub fn reset_settings(base_dir: &Path) -> Result<AppSettings, SettingsStoreError> {
    let defaults = AppSettings::default();
    save_settings(base_dir, &defaults)?;
    Ok(defaults)
}
