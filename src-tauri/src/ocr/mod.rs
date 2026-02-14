use std::io::Cursor;
use std::sync::{Mutex, OnceLock};
use std::time::Duration;

use base64::Engine as _;
use reqwest::header::{AUTHORIZATION, CONTENT_TYPE};
use serde::{Deserialize, Serialize};
use tauri::{
    AppHandle, Emitter, Manager, PhysicalPosition, PhysicalSize, Position, Size,
};
use tauri_plugin_global_shortcut::{GlobalShortcutExt, ShortcutState};

use crate::settings::model::{AppSettings, OcrProvider, OcrSettings};
use crate::settings::store;
use crate::windows::ids::WindowKind;
use crate::windows::manager;

fn ocr_hotkey_store() -> &'static Mutex<Option<String>> {
    static STORE: OnceLock<Mutex<Option<String>>> = OnceLock::new();
    STORE.get_or_init(|| Mutex::new(None))
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct OcrCaptureRegion {
    pub x: f64,
    pub y: f64,
    pub width: f64,
    pub height: f64,
    pub scale_factor: f64,
}

#[derive(Debug, Clone, Copy)]
struct PhysicalCaptureRect {
    x: i32,
    y: i32,
    width: u32,
    height: u32,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
struct ChangeTextPayload {
    text: String,
    source: &'static str,
    target: &'static str,
    title: Option<String>,
    custom_prompt: Option<String>,
    custom_model: Option<String>,
    request_id: Option<u64>,
    ocr_stage: Option<&'static str>,
}

#[derive(Debug, Clone)]
struct OcrActionRoute {
    target_window: WindowKind,
    target: &'static str,
    title: Option<String>,
    custom_prompt: Option<String>,
    custom_model: Option<String>,
}

#[derive(Debug, thiserror::Error)]
enum OcrError {
    #[error("OCR 未启用，请先在设置中开启")]
    Disabled,
    #[error("OCR API Key 为空")]
    EmptyApiKey,
    #[error("OCR 模型名称为空")]
    EmptyModel,
    #[error("OCR 选区无效")]
    InvalidRegion,
    #[error("OCR 截图窗口不可用")]
    CaptureWindowUnavailable,
    #[error("当前未找到可用显示器")]
    MonitorUnavailable,
    #[error("截图失败：{0}")]
    CaptureFailure(String),
    #[error("网络请求失败：{0}")]
    Http(#[from] reqwest::Error),
    #[error("图像编码失败：{0}")]
    Encode(String),
    #[error("OCR 响应为空")]
    EmptyResponse,
    #[error("读取设置失败：{0}")]
    Settings(String),
}

#[derive(Debug, Serialize)]
struct VisionChatRequest {
    model: String,
    temperature: f32,
    messages: Vec<VisionMessage>,
}

#[derive(Debug, Serialize)]
struct VisionMessage {
    role: &'static str,
    content: Vec<VisionContent>,
}

#[derive(Debug, Serialize)]
#[serde(tag = "type", rename_all = "snake_case")]
enum VisionContent {
    Text { text: String },
    ImageUrl { image_url: VisionImageUrl },
}

#[derive(Debug, Serialize)]
struct VisionImageUrl {
    url: String,
}

pub fn sync_ocr_hotkey(app: &AppHandle, settings: &AppSettings) -> Result<(), String> {
    let desired = if settings.ocr.enabled {
        let hotkey = settings.ocr.capture_hotkey.trim();
        if hotkey.is_empty() {
            None
        } else {
            Some(hotkey.to_owned())
        }
    } else {
        None
    };

    let shortcut_api = app.global_shortcut();

    let mut guard = ocr_hotkey_store()
        .lock()
        .map_err(|error| format!("failed to lock OCR hotkey store: {error}"))?;

    if *guard == desired {
        return Ok(());
    }

    if let Some(current) = guard.as_ref() {
        let _ = shortcut_api.unregister(current.as_str());
    }

    if let Some(next_hotkey) = desired.clone() {
        shortcut_api
            .on_shortcut(next_hotkey.as_str(), |app_handle, _shortcut, event| {
                if event.state != ShortcutState::Pressed {
                    return;
                }

                if let Err(error) = open_capture_overlay(app_handle) {
                    eprintln!("failed to open OCR capture overlay: {error}");
                }
            })
            .map_err(|error| format!("failed to register OCR hotkey `{next_hotkey}`: {error}"))?;
    }

    *guard = desired;
    Ok(())
}

pub fn open_capture_overlay(app: &AppHandle) -> Result<(), String> {
    let window = manager::ensure_window(app, WindowKind::OcrCapture)
        .map_err(|error| format!("failed to create OCR capture window: {error}"))?;

    let cursor = app
        .cursor_position()
        .map_err(|error| format!("failed to read cursor position: {error}"))?;

    let monitor = app
        .monitor_from_point(cursor.x, cursor.y)
        .map_err(|error| format!("failed to resolve monitor from cursor: {error}"))?
        .or_else(|| app.primary_monitor().ok().flatten())
        .ok_or_else(|| String::from("no monitor available for OCR capture"))?;

    let monitor_position = monitor.position();
    let monitor_size = monitor.size();

    window
        .set_position(Position::Physical(PhysicalPosition::new(
            monitor_position.x,
            monitor_position.y,
        )))
        .map_err(|error| format!("failed to position OCR capture window: {error}"))?;

    window
        .set_size(Size::Physical(PhysicalSize::new(
            monitor_size.width,
            monitor_size.height,
        )))
        .map_err(|error| format!("failed to resize OCR capture window: {error}"))?;

    window
        .show()
        .map_err(|error| format!("failed to show OCR capture window: {error}"))?;

    let _ = app.emit_to(WindowKind::OcrCapture.label(), "ocr-capture-opened", ());

    let _ = window.unminimize();
    let _ = window.set_focus();

    Ok(())
}

pub async fn run_ocr_capture(app: &AppHandle, region: OcrCaptureRegion) -> Result<(), String> {
    let result = run_ocr_capture_inner(app, region).await;
    let _ = manager::hide_window(app, WindowKind::OcrCapture);
    result.map_err(|error| error.to_string())
}

async fn run_ocr_capture_inner(app: &AppHandle, region: OcrCaptureRegion) -> Result<(), OcrError> {
    let config_root = app
        .path()
        .app_config_dir()
        .map_err(|error| OcrError::Settings(format!("failed to resolve app config dir: {error}")))?;

    let settings = store::load_settings(&config_root)
        .map_err(|error| OcrError::Settings(error.to_string()))?;

    if !settings.ocr.enabled {
        return Err(OcrError::Disabled);
    }

    let route = resolve_ocr_action_route(&settings);

    let capture_window = app
        .get_webview_window(WindowKind::OcrCapture.label())
        .ok_or(OcrError::CaptureWindowUnavailable)?;

    let physical_rect = logical_region_to_physical(&capture_window, &region)?;

    let _ = manager::hide_window(app, WindowKind::OcrCapture);
    std::thread::sleep(Duration::from_millis(90));

    let image_data_url = capture_region_data_url(physical_rect)?;

    manager::show_window(app, route.target_window)
        .map_err(|error| OcrError::CaptureFailure(error.to_string()))?;
    emit_change_text(app, &route, String::new(), Some("ocring"), None)?;

    let text = match request_ocr_text(&settings.ocr, &image_data_url).await {
        Ok(value) => value,
        Err(error) => {
            let _ = emit_change_text(app, &route, String::new(), Some("idle"), None);
            return Err(error);
        }
    };

    emit_change_text(
        app,
        &route,
        text,
        Some("processing"),
        Some(current_request_id()),
    )?;

    Ok(())
}

fn logical_region_to_physical(
    window: &tauri::WebviewWindow,
    region: &OcrCaptureRegion,
) -> Result<PhysicalCaptureRect, OcrError> {
    if !region.width.is_finite()
        || !region.height.is_finite()
        || region.width <= 0.0
        || region.height <= 0.0
    {
        return Err(OcrError::InvalidRegion);
    }

    let monitor = window
        .current_monitor()
        .map_err(|error| OcrError::CaptureFailure(error.to_string()))?
        .ok_or(OcrError::MonitorUnavailable)?;

    let monitor_position = monitor.position();
    let monitor_size = monitor.size();
    let scale = if region.scale_factor.is_finite() && region.scale_factor > 0.0 {
        region.scale_factor
    } else {
        monitor.scale_factor()
    };

    let monitor_width = i32::try_from(monitor_size.width).unwrap_or(i32::MAX);
    let monitor_height = i32::try_from(monitor_size.height).unwrap_or(i32::MAX);

    let raw_left = (region.x.max(0.0) * scale).round() as i32;
    let raw_top = (region.y.max(0.0) * scale).round() as i32;
    let raw_width = ((region.width * scale).round() as i32).max(1);
    let raw_height = ((region.height * scale).round() as i32).max(1);

    let left = raw_left.clamp(0, monitor_width.saturating_sub(1));
    let top = raw_top.clamp(0, monitor_height.saturating_sub(1));
    let max_width = monitor_width.saturating_sub(left).max(1);
    let max_height = monitor_height.saturating_sub(top).max(1);
    let width = raw_width.min(max_width).max(1) as u32;
    let height = raw_height.min(max_height).max(1) as u32;

    Ok(PhysicalCaptureRect {
        x: monitor_position.x + left,
        y: monitor_position.y + top,
        width,
        height,
    })
}

fn capture_region_data_url(rect: PhysicalCaptureRect) -> Result<String, OcrError> {
    #[cfg(target_os = "windows")]
    {
        let screen = screenshots::Screen::from_point(rect.x + 1, rect.y + 1)
            .map_err(|error| OcrError::CaptureFailure(error.to_string()))?;

        let relative_x = rect.x - screen.display_info.x;
        let relative_y = rect.y - screen.display_info.y;

        let image = screen
            .capture_area_ignore_area_check(relative_x, relative_y, rect.width, rect.height)
            .map_err(|error| OcrError::CaptureFailure(error.to_string()))?;

        let mut cursor = Cursor::new(Vec::<u8>::new());
        screenshots::image::DynamicImage::ImageRgba8(image)
            .write_to(&mut cursor, screenshots::image::ImageOutputFormat::Png)
            .map_err(|error| OcrError::Encode(error.to_string()))?;

        let encoded = base64::engine::general_purpose::STANDARD.encode(cursor.into_inner());
        return Ok(format!("data:image/png;base64,{encoded}"));
    }

    #[cfg(not(target_os = "windows"))]
    {
        let _ = rect;
        Err(OcrError::CaptureFailure(String::from(
            "OCR capture is currently supported on Windows only",
        )))
    }
}

async fn request_ocr_text(ocr: &OcrSettings, image_data_url: &str) -> Result<String, OcrError> {
    if ocr.api_key.trim().is_empty() {
        return Err(OcrError::EmptyApiKey);
    }

    if ocr.model.trim().is_empty() {
        return Err(OcrError::EmptyModel);
    }

    let timeout_ms = ocr.timeout_ms.clamp(1_000, 120_000);
    let prompt = if ocr.prompt.trim().is_empty() {
        String::from("请提取图片中的全部文字，按原有顺序输出，不要添加解释。")
    } else {
        ocr.prompt.trim().to_owned()
    };

    let system_prompt = match ocr.provider {
        OcrProvider::OpenaiVision => {
            "You are an OCR engine. Extract all visible text from the image and return plain text only."
        }
        OcrProvider::GlmOcr => {
            "你是OCR引擎。请识别图片中的所有文字并按阅读顺序输出纯文本，不要添加解释。"
        }
    };

    let request_body = VisionChatRequest {
        model: ocr.model.trim().to_owned(),
        temperature: 0.0,
        messages: vec![
            VisionMessage {
                role: "system",
                content: vec![VisionContent::Text {
                    text: system_prompt.to_owned(),
                }],
            },
            VisionMessage {
                role: "user",
                content: vec![
                    VisionContent::Text { text: prompt },
                    VisionContent::ImageUrl {
                        image_url: VisionImageUrl {
                            url: image_data_url.to_owned(),
                        },
                    },
                ],
            },
        ],
    };

    let client = reqwest::Client::builder()
        .timeout(std::time::Duration::from_millis(timeout_ms))
        .build()?;

    let response = client
        .post(build_ocr_chat_url(ocr))
        .header(CONTENT_TYPE, "application/json")
        .header(AUTHORIZATION, format!("Bearer {}", ocr.api_key.trim()))
        .json(&request_body)
        .send()
        .await?
        .error_for_status()?
        .json::<serde_json::Value>()
        .await?;

    let text = extract_ocr_text_from_response(&response).ok_or(OcrError::EmptyResponse)?;
    let trimmed = text.trim();

    if trimmed.is_empty() {
        return Err(OcrError::EmptyResponse);
    }

    Ok(trimmed.to_owned())
}

fn build_ocr_chat_url(ocr: &OcrSettings) -> String {
    let trimmed = ocr.base_url.trim().trim_end_matches('/');

    if trimmed.ends_with("/chat/completions") {
        return trimmed.to_owned();
    }

    if matches!(ocr.provider, OcrProvider::GlmOcr) {
        if trimmed == "https://open.bigmodel.cn" {
            return String::from("https://open.bigmodel.cn/api/paas/v4/chat/completions");
        }
        if trimmed.ends_with("/api/paas/v4") {
            return format!("{trimmed}/chat/completions");
        }
    }

    format!("{trimmed}/chat/completions")
}

fn extract_ocr_text_from_response(payload: &serde_json::Value) -> Option<String> {
    let content = payload
        .get("choices")?
        .as_array()?
        .first()?
        .get("message")?
        .get("content")?;

    if let Some(text) = content.as_str() {
        return Some(text.to_owned());
    }

    if let Some(items) = content.as_array() {
        let joined = items
            .iter()
            .filter_map(|item| {
                item.get("text")
                    .and_then(|value| value.as_str())
                    .map(|value| value.trim().to_owned())
                    .filter(|value| !value.is_empty())
            })
            .collect::<Vec<_>>()
            .join("\n");

        if !joined.trim().is_empty() {
            return Some(joined);
        }
    }

    None
}

fn resolve_ocr_action_route(settings: &AppSettings) -> OcrActionRoute {
    let action_id = settings.ocr.post_action_id.trim();

    match action_id {
        "translate" => OcrActionRoute {
            target_window: WindowKind::Translate,
            target: "translate",
            title: None,
            custom_prompt: None,
            custom_model: None,
        },
        "summary" | "summarize" => OcrActionRoute {
            target_window: WindowKind::Summary,
            target: "summary",
            title: None,
            custom_prompt: None,
            custom_model: None,
        },
        "explain" => OcrActionRoute {
            target_window: WindowKind::Explain,
            target: "explain",
            title: None,
            custom_prompt: None,
            custom_model: None,
        },
        "optimize" => OcrActionRoute {
            target_window: WindowKind::Optimize,
            target: "optimize",
            title: Some(String::from("优化")),
            custom_prompt: None,
            custom_model: None,
        },
        _ => {
            if settings.features.custom_actions_enabled {
                if let Some(custom) = settings
                    .features
                    .custom_actions
                    .iter()
                    .find(|item| item.enabled && item.id.trim() == action_id)
                {
                    return OcrActionRoute {
                        target_window: WindowKind::Optimize,
                        target: "optimize",
                        title: if custom.name.trim().is_empty() {
                            Some(String::from("优化"))
                        } else {
                            Some(custom.name.trim().to_owned())
                        },
                        custom_prompt: if custom.prompt.trim().is_empty() {
                            None
                        } else {
                            Some(custom.prompt.trim().to_owned())
                        },
                        custom_model: if custom.model.trim().is_empty() {
                            None
                        } else {
                            Some(custom.model.trim().to_owned())
                        },
                    };
                }
            }

            OcrActionRoute {
                target_window: WindowKind::Translate,
                target: "translate",
                title: None,
                custom_prompt: None,
                custom_model: None,
            }
        }
    }
}

fn emit_change_text(
    app: &AppHandle,
    route: &OcrActionRoute,
    text: String,
    ocr_stage: Option<&'static str>,
    request_id: Option<u64>,
) -> Result<(), OcrError> {
    app.emit_to(
        route.target_window.label(),
        "change-text",
        ChangeTextPayload {
            text,
            source: "ocr",
            target: route.target,
            title: route.title.clone(),
            custom_prompt: route.custom_prompt.clone(),
            custom_model: route.custom_model.clone(),
            request_id,
            ocr_stage,
        },
    )
    .map_err(|error| OcrError::CaptureFailure(error.to_string()))
}

fn current_request_id() -> u64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map(|duration| duration.as_millis() as u64)
        .unwrap_or(0)
}
