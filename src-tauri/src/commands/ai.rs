use tauri::Manager;

use crate::ai::client;
use crate::ai::prompts::{TaskKind, TaskOptions};
use crate::settings::model::ApiSettings;
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

/// Start a streaming API call. Returns the stream_id immediately so the
/// frontend can listen for `stream-chunk`, `stream-done`, and `stream-error`
/// events filtered by that id.
#[tauri::command]
pub async fn stream_process_text(
    app: tauri::AppHandle,
    task_kind: TaskKind,
    text: String,
    options: Option<TaskOptions>,
) -> Result<String, String> {
    let config_root = app
        .path()
        .app_config_dir()
        .map_err(|error| format!("failed to resolve app config dir: {error}"))?;

    let settings = store::load_settings(&config_root).map_err(|error| error.to_string())?;

    let stream_id = uuid::Uuid::new_v4().to_string();
    let sid = stream_id.clone();

    // Spawn the streaming work on a background task so this command returns
    // the stream_id immediately.
    let app_clone = app.clone();
    tauri::async_runtime::spawn(async move {
        use tauri::Emitter;

        if let Err(error) = client::process_text_stream(
            &app_clone,
            &settings.api,
            task_kind,
            &text,
            options.as_ref(),
            sid.clone(),
        )
        .await
        {
            let _ = app_clone.emit(
                "stream-error",
                client::StreamErrorPayload {
                    stream_id: sid,
                    error: error.to_string(),
                },
            );
        }
    });

    Ok(stream_id)
}

#[tauri::command]
pub async fn test_api_connection(api: ApiSettings) -> Result<client::ApiConnectionTest, String> {
    client::test_api_connection(&api)
        .await
        .map_err(|error| error.to_string())
}
