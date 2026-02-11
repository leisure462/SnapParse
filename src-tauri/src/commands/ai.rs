use tauri::Manager;

use crate::ai::client;
use crate::ai::prompts::{TaskKind, TaskOptions};
use crate::settings::store;

#[tauri::command]
pub async fn process_selected_text(
    app: tauri::AppHandle,
    task_kind: TaskKind,
    text: String,
    options: Option<TaskOptions>,
) -> Result<client::ProcessedText, String> {
    let config_root = app
        .path()
        .app_config_dir()
        .map_err(|error| format!("failed to resolve app config dir: {error}"))?;

    let settings = store::load_settings(&config_root).map_err(|error| error.to_string())?;

    client::process_text(&settings.api, task_kind, &text, options.as_ref())
        .await
        .map_err(|error| error.to_string())
}
