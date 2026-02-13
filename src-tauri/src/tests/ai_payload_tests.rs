use crate::ai::client::build_chat_completions_url;
use crate::ai::prompts::{build_prompt, TaskKind, TaskOptions};

#[test]
fn summarize_task_uses_summary_prompt_template() {
    let prompt = build_prompt(TaskKind::Summarize, "hello", None);
    assert!(prompt.system.contains("总结"));
}

#[test]
fn translate_task_includes_source_and_target_languages() {
    let prompt = build_prompt(
        TaskKind::Translate,
        "hello",
        Some(&TaskOptions {
            from_language: Some(String::from("zh-CN")),
            to_language: Some(String::from("en")),
            language: None,
            target_length: None,
            custom_prompt: None,
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

#[test]
fn optimize_task_uses_optimize_prompt_template() {
    let prompt = build_prompt(TaskKind::Optimize, "hello", None);
    assert!(prompt.system.contains("润色"));
}

#[test]
fn custom_prompt_replaces_placeholders() {
    let prompt = build_prompt(
        TaskKind::Optimize,
        "hello world",
        Some(&TaskOptions {
            from_language: Some(String::from("en")),
            to_language: Some(String::from("zh-CN")),
            language: None,
            target_length: None,
            custom_prompt: Some(String::from("SRC={{language}} DST={{target_language}} TEXT={{text}}")),
        }),
    );

    assert!(prompt.system.contains("SRC=en"));
    assert!(prompt.system.contains("DST=zh-CN"));
    assert!(prompt.system.contains("TEXT=hello world"));
}

#[test]
fn optimize_task_wraps_user_text_in_input_xml() {
    let prompt = build_prompt(TaskKind::Optimize, "hello", None);
    assert!(prompt.user.contains("<INPUT>"));
    assert!(prompt.user.contains("</INPUT>"));
}

#[test]
fn translate_task_wraps_user_text_in_translate_input_xml() {
    let prompt = build_prompt(TaskKind::Translate, "hello", None);
    assert!(prompt.user.contains("<translate_input>"));
    assert!(prompt.user.contains("</translate_input>"));
}

#[test]
fn summarize_task_uses_requested_ui_language() {
    let prompt = build_prompt(
        TaskKind::Summarize,
        "hello",
        Some(&TaskOptions {
            from_language: None,
            to_language: None,
            language: Some(String::from("en-US")),
            target_length: None,
            custom_prompt: None,
        }),
    );

    assert!(prompt.system.contains("en-US"));
}
