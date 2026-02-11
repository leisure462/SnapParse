use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, Serialize, Deserialize, Eq, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum TaskKind {
    Translate,
    Summarize,
    Explain,
}

#[derive(Debug, Clone, Serialize, Deserialize, Default)]
#[serde(rename_all = "camelCase")]
pub struct TaskOptions {
    pub from_language: Option<String>,
    pub to_language: Option<String>,
    pub target_length: Option<String>,
}

#[derive(Debug, Clone)]
pub struct PromptBundle {
    pub system: String,
    pub user: String,
}

pub fn build_prompt(task_kind: TaskKind, text: &str, options: Option<&TaskOptions>) -> PromptBundle {
    let user = text.trim().to_owned();

    let system = match task_kind {
        TaskKind::Translate => {
            let from = options
                .and_then(|item| item.from_language.as_deref())
                .unwrap_or("auto");
            let to = options
                .and_then(|item| item.to_language.as_deref())
                .unwrap_or("zh-CN");

            format!(
                "You are a professional translation assistant. Translate faithfully from {from} to {to}. Keep formatting and tone. If the text is ambiguous, provide the most likely translation first."
            )
        }
        TaskKind::Summarize => {
            let length = options
                .and_then(|item| item.target_length.as_deref())
                .unwrap_or("short");

            format!(
                "You are a concise summary assistant. Produce a {length} summary in clear Chinese, preserving key facts and intent."
            )
        }
        TaskKind::Explain => {
            "You are a teaching assistant. Explain the selected text in simple Chinese with key points and context.".to_owned()
        }
    };

    PromptBundle { system, user }
}
