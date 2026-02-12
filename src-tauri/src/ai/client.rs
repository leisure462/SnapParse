use reqwest::header::{AUTHORIZATION, CONTENT_TYPE};
use serde::{Deserialize, Serialize};

use crate::ai::prompts::{build_prompt, TaskKind, TaskOptions};
use crate::settings::model::ApiSettings;

#[derive(Debug, thiserror::Error)]
pub enum AiClientError {
    #[error("api key is empty")]
    EmptyApiKey,
    #[error("http error: {0}")]
    Http(#[from] reqwest::Error),
    #[error("api response missing choices")]
    MissingChoices,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ProcessedText {
    pub task_kind: TaskKind,
    pub source_text: String,
    pub result_text: String,
    pub used_model: String,
    pub elapsed_ms: u128,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ApiConnectionTest {
    pub model: String,
    pub message: String,
    pub elapsed_ms: u128,
}

// ── Request / Response types ────────────────────────────────────

#[derive(Debug, Serialize)]
struct ChatCompletionsRequest {
    model: String,
    temperature: f32,
    messages: Vec<ChatMessage>,
    #[serde(skip_serializing_if = "Option::is_none")]
    stream: Option<bool>,
}

#[derive(Debug, Serialize)]
struct ChatMessage {
    role: &'static str,
    content: String,
}

#[derive(Debug, Deserialize)]
struct ChatCompletionsResponse {
    choices: Vec<ChoiceItem>,
}

#[derive(Debug, Deserialize)]
struct ChoiceItem {
    message: ChatMessageResponse,
}

#[derive(Debug, Deserialize)]
struct ChatMessageResponse {
    content: String,
}

// ── SSE streaming response types ────────────────────────────────

#[derive(Debug, Deserialize)]
struct StreamChunkResponse {
    choices: Vec<StreamChoiceItem>,
}

#[derive(Debug, Deserialize)]
struct StreamChoiceItem {
    delta: StreamDelta,
}

#[derive(Debug, Deserialize)]
struct StreamDelta {
    #[serde(default)]
    content: Option<String>,
}

// ── Tauri event payloads ────────────────────────────────────────

/// Emitted for each text chunk during streaming.
#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct StreamChunkPayload {
    pub stream_id: String,
    pub chunk: String,
}

/// Emitted when streaming completes.
#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct StreamDonePayload {
    pub stream_id: String,
    pub full_text: String,
    pub elapsed_ms: u128,
}

/// Emitted when streaming fails.
#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct StreamErrorPayload {
    pub stream_id: String,
    pub error: String,
}

// ── Helpers ─────────────────────────────────────────────────────

pub fn build_chat_completions_url(base_url: &str) -> String {
    let trimmed = base_url.trim_end_matches('/');

    if trimmed.ends_with("/chat/completions") {
        trimmed.to_owned()
    } else {
        format!("{trimmed}/chat/completions")
    }
}

// ── Non-streaming API call (kept for test_api_connection) ───────

pub async fn test_api_connection(
    api_settings: &ApiSettings,
) -> Result<ApiConnectionTest, AiClientError> {
    if api_settings.api_key.trim().is_empty() {
        return Err(AiClientError::EmptyApiKey);
    }

    let started = std::time::Instant::now();
    let client = reqwest::Client::builder()
        .timeout(std::time::Duration::from_millis(api_settings.timeout_ms))
        .build()?;

    let request_body = ChatCompletionsRequest {
        model: api_settings.model.clone(),
        temperature: 0.0,
        stream: None,
        messages: vec![
            ChatMessage {
                role: "system",
                content: String::from("You are an API connectivity checker. Reply briefly."),
            },
            ChatMessage {
                role: "user",
                content: String::from("Respond with: API connection successful."),
            },
        ],
    };

    let response = client
        .post(build_chat_completions_url(&api_settings.base_url))
        .header(CONTENT_TYPE, "application/json")
        .header(AUTHORIZATION, format!("Bearer {}", api_settings.api_key.trim()))
        .json(&request_body)
        .send()
        .await?
        .error_for_status()?
        .json::<ChatCompletionsResponse>()
        .await?;

    let message = response
        .choices
        .first()
        .map(|item| item.message.content.trim().to_owned())
        .filter(|item| !item.is_empty())
        .ok_or(AiClientError::MissingChoices)?;

    Ok(ApiConnectionTest {
        model: api_settings.model.clone(),
        message,
        elapsed_ms: started.elapsed().as_millis(),
    })
}

// ── Non-streaming process (fallback) ────────────────────────────

pub async fn process_text(
    api_settings: &ApiSettings,
    task_kind: TaskKind,
    source_text: &str,
    options: Option<&TaskOptions>,
) -> Result<ProcessedText, AiClientError> {
    if api_settings.api_key.trim().is_empty() {
        return Err(AiClientError::EmptyApiKey);
    }

    let started = std::time::Instant::now();
    let prompt = build_prompt(task_kind, source_text, options);
    let client = reqwest::Client::builder()
        .timeout(std::time::Duration::from_millis(api_settings.timeout_ms))
        .build()?;

    let request_body = ChatCompletionsRequest {
        model: api_settings.model.clone(),
        temperature: api_settings.temperature,
        stream: None,
        messages: vec![
            ChatMessage {
                role: "system",
                content: prompt.system,
            },
            ChatMessage {
                role: "user",
                content: prompt.user,
            },
        ],
    };

    let response = client
        .post(build_chat_completions_url(&api_settings.base_url))
        .header(CONTENT_TYPE, "application/json")
        .header(AUTHORIZATION, format!("Bearer {}", api_settings.api_key.trim()))
        .json(&request_body)
        .send()
        .await?
        .error_for_status()?
        .json::<ChatCompletionsResponse>()
        .await?;

    let result_text = response
        .choices
        .first()
        .map(|item| item.message.content.trim().to_owned())
        .filter(|item| !item.is_empty())
        .ok_or(AiClientError::MissingChoices)?;

    Ok(ProcessedText {
        task_kind,
        source_text: source_text.to_owned(),
        result_text,
        used_model: api_settings.model.clone(),
        elapsed_ms: started.elapsed().as_millis(),
    })
}

// ── Streaming process ───────────────────────────────────────────

/// Sends a streaming request to the OpenAI-compatible API and emits
/// Tauri events for each chunk. Returns the stream_id so the caller
/// can correlate events.
pub async fn process_text_stream(
    app: &tauri::AppHandle,
    api_settings: &ApiSettings,
    task_kind: TaskKind,
    source_text: &str,
    options: Option<&TaskOptions>,
    stream_id: String,
) -> Result<(), AiClientError> {
    use tauri::Emitter;

    if api_settings.api_key.trim().is_empty() {
        return Err(AiClientError::EmptyApiKey);
    }

    let started = std::time::Instant::now();
    let prompt = build_prompt(task_kind, source_text, options);
    let client = reqwest::Client::builder()
        .timeout(std::time::Duration::from_millis(api_settings.timeout_ms))
        .build()?;

    let request_body = ChatCompletionsRequest {
        model: api_settings.model.clone(),
        temperature: api_settings.temperature,
        stream: Some(true),
        messages: vec![
            ChatMessage {
                role: "system",
                content: prompt.system,
            },
            ChatMessage {
                role: "user",
                content: prompt.user,
            },
        ],
    };

    let mut response = client
        .post(build_chat_completions_url(&api_settings.base_url))
        .header(CONTENT_TYPE, "application/json")
        .header(AUTHORIZATION, format!("Bearer {}", api_settings.api_key.trim()))
        .json(&request_body)
        .send()
        .await?
        .error_for_status()?;

    let mut full_text = String::new();
    let mut line_buffer = String::new();

    // Read response body chunk by chunk using reqwest's .chunk() method
    while let Some(chunk_bytes) = response.chunk().await? {
        let chunk_str = String::from_utf8_lossy(&chunk_bytes);
        line_buffer.push_str(&chunk_str);

        // Process all complete lines in the buffer
        while let Some(newline_pos) = line_buffer.find('\n') {
            let line = line_buffer[..newline_pos].trim().to_owned();
            line_buffer = line_buffer[newline_pos + 1..].to_owned();

            if line.is_empty() {
                continue;
            }

            // SSE format: "data: {...}" or "data: [DONE]"
            if let Some(data) = line.strip_prefix("data: ") {
                let data = data.trim();
                if data == "[DONE]" {
                    break;
                }

                if let Ok(parsed) = serde_json::from_str::<StreamChunkResponse>(data) {
                    if let Some(choice) = parsed.choices.first() {
                        if let Some(content) = &choice.delta.content {
                            if !content.is_empty() {
                                full_text.push_str(content);
                                let _ = app.emit("stream-chunk", StreamChunkPayload {
                                    stream_id: stream_id.clone(),
                                    chunk: content.clone(),
                                });
                            }
                        }
                    }
                }
            }
        }
    }

    let _ = app.emit("stream-done", StreamDonePayload {
        stream_id,
        full_text,
        elapsed_ms: started.elapsed().as_millis(),
    });

    Ok(())
}
