use std::fs;
use std::path::PathBuf;
use std::time::{SystemTime, UNIX_EPOCH};

use crate::settings::model::AppSettings;
use crate::settings::store::{load_settings, reset_settings, save_settings};

fn make_temp_config_root(suffix: &str) -> PathBuf {
    let timestamp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .expect("clock drift")
        .as_nanos();

    let root = std::env::temp_dir().join(format!(
        "snapparse-settings-{suffix}-{}-{timestamp}",
        std::process::id()
    ));

    fs::create_dir_all(&root).expect("failed to create temp root");
    root
}

#[test]
fn settings_roundtrip_persists_plaintext_api_key() {
    let root = make_temp_config_root("roundtrip");

    let mut settings = AppSettings::default();
    settings.api.api_key = String::from("sk-test-123");

    save_settings(&root, &settings).expect("save should succeed");
    let loaded = load_settings(&root).expect("load should succeed");

    assert_eq!(loaded.api.api_key, "sk-test-123");

    fs::remove_dir_all(root).ok();
}

#[test]
fn load_returns_default_when_missing() {
    let root = make_temp_config_root("missing");

    let loaded = load_settings(&root).expect("load should succeed");
    assert_eq!(loaded, AppSettings::default());

    fs::remove_dir_all(root).ok();
}

#[test]
fn reset_overwrites_saved_settings_with_defaults() {
    let root = make_temp_config_root("reset");

    let mut settings = AppSettings::default();
    settings.api.model = String::from("custom-model");
    save_settings(&root, &settings).expect("save should succeed");

    let reset = reset_settings(&root).expect("reset should succeed");
    let loaded = load_settings(&root).expect("load should succeed");

    assert_eq!(reset, AppSettings::default());
    assert_eq!(loaded, AppSettings::default());

    fs::remove_dir_all(root).ok();
}
