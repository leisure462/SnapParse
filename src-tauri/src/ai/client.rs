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

#[derive(Debug, Serialize)]
struct ChatCompletionsRequest {
    model: String,
    temperature: f32,
    messages: Vec<ChatMessage>,
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

pub fn build_chat_completions_url(base_url: &str) -> String {
    let trimmed = base_url.trim_end_matches('/');

    if trimmed.ends_with("/chat/completions") {
        trimmed.to_owned()
    } else {
        format!("{trimmed}/chat/completions")
    }
}

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
