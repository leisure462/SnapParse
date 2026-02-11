use crate::ai::client::build_chat_completions_url;
use crate::ai::prompts::{build_prompt, TaskKind, TaskOptions};

#[test]
fn summarize_task_uses_summary_prompt_template() {
    let prompt = build_prompt(TaskKind::Summarize, "hello", None);
    assert!(prompt.system.to_lowercase().contains("summary"));
}

#[test]
fn translate_task_includes_source_and_target_languages() {
    let prompt = build_prompt(
        TaskKind::Translate,
        "hello",
        Some(&TaskOptions {
            from_language: Some(String::from("zh-CN")),
            to_language: Some(String::from("en")),
            target_length: None,
        }),
    );

    assert!(prompt.system.contains("zh-CN"));
    assert!(prompt.system.contains("en"));
}

#[test]
fn builds_expected_chat_completion_url() {
    assert_eq!(
        build_chat_completions_url("https://api.openai.com/v1"),
        "https://api.openai.com/v1/chat/completions"
    );
}
