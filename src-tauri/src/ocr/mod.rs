use std::collections::HashSet;
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

use crate::settings::model::{
    AppSettings, CaptureMode as SettingsCaptureMode, OcrProvider, OcrSettings, WindowSizePreset,
};
use crate::settings::store;
use crate::windows::ids::WindowKind;
use crate::windows::manager;

const GLM_OCR_LAYOUT_URL: &str = "https://open.bigmodel.cn/api/paas/v4/layout_parsing";
const GLM_OCR_FIXED_MODEL: &str = "glm-ocr";

#[derive(Debug, Clone, Default, Eq, PartialEq)]
struct RegisteredOcrHotkeys {
    screenshot_hotkey: Option<String>,
    fullscreen_hotkey: Option<String>,
    window_hotkey: Option<String>,
    quick_ocr_hotkey: Option<String>,
}

#[derive(Debug, Copy, Clone)]
enum CaptureEntryKind {
    Screenshot,
    Ocr,
}

impl CaptureEntryKind {
    fn as_str(self) -> &'static str {
        match self {
            CaptureEntryKind::Screenshot => "screenshot",
            CaptureEntryKind::Ocr => "ocr",
        }
    }
}

fn ocr_hotkey_store() -> &'static Mutex<RegisteredOcrHotkeys> {
    static STORE: OnceLock<Mutex<RegisteredOcrHotkeys>> = OnceLock::new();
    STORE.get_or_init(|| Mutex::new(RegisteredOcrHotkeys::default()))
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

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct CapturePoint {
    pub x: f64,
    pub y: f64,
    pub scale_factor: f64,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ScreenshotCaptureRequest {
    pub mode: String,
    pub region: Option<OcrCaptureRegion>,
    pub point: Option<CapturePoint>,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct LogicalRectPayload {
    pub x: f64,
    pub y: f64,
    pub width: f64,
    pub height: f64,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ScreenshotPreviewPayload {
    pub data_url: String,
    pub logical_rect: LogicalRectPayload,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
struct CaptureOpenedPayload {
    entry_kind: &'static str,
    initial_mode: Option<&'static str>,
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
    #[error("截屏模式不支持：{0}")]
    UnsupportedMode(String),
    #[error("未找到可截取窗口")]
    WindowNotFound,
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

#[derive(Debug, Serialize)]
struct GlmLayoutRequest {
    model: &'static str,
    file: String,
}

pub fn sync_ocr_hotkey(app: &AppHandle, settings: &AppSettings) -> Result<(), String> {
    let desired = if settings.ocr.enabled {
        RegisteredOcrHotkeys {
            screenshot_hotkey: {
                let hotkey = settings.ocr.capture_hotkey.trim();
                if hotkey.is_empty() {
                    None
                } else {
                    Some(hotkey.to_owned())
                }
            },
            fullscreen_hotkey: {
                let hotkey = settings.ocr.mode_hotkeys.fullscreen.trim();
                if hotkey.is_empty() {
                    None
                } else {
                    Some(hotkey.to_owned())
                }
            },
            window_hotkey: {
                let hotkey = settings.ocr.mode_hotkeys.window.trim();
                if hotkey.is_empty() {
                    None
                } else {
                    Some(hotkey.to_owned())
                }
            },
            quick_ocr_hotkey: {
                let hotkey = settings.ocr.quick_ocr_hotkey.trim();
                if hotkey.is_empty() {
                    None
                } else {
                    Some(hotkey.to_owned())
                }
            },
        }
    } else {
        RegisteredOcrHotkeys::default()
    };

    let shortcut_api = app.global_shortcut();

    let mut guard = ocr_hotkey_store()
        .lock()
        .map_err(|error| format!("failed to lock OCR hotkey store: {error}"))?;

    if *guard == desired {
        return Ok(());
    }

    let mut removed = HashSet::new();
    for hotkey in [
        guard.screenshot_hotkey.as_ref(),
        guard.fullscreen_hotkey.as_ref(),
        guard.window_hotkey.as_ref(),
        guard.quick_ocr_hotkey.as_ref(),
    ] {
        if let Some(current) = hotkey {
            let normalized = current.to_ascii_lowercase();
            if removed.insert(normalized) {
                let _ = shortcut_api.unregister(current.as_str());
            }
        }
    }

    let mut registered = HashSet::new();

    if let Some(next_hotkey) = desired.quick_ocr_hotkey.as_ref() {
        registered.insert(next_hotkey.to_ascii_lowercase());
        register_hotkey(
            app,
            next_hotkey,
            CaptureEntryKind::Ocr,
            Some(SettingsCaptureMode::Region),
            "ocr hotkey",
        )?;
    }

    if let Some(next_hotkey) = desired.fullscreen_hotkey.as_ref() {
        let normalized = next_hotkey.to_ascii_lowercase();
        if registered.insert(normalized) {
            register_hotkey(
                app,
                next_hotkey,
                CaptureEntryKind::Screenshot,
                Some(SettingsCaptureMode::Fullscreen),
                "fullscreen screenshot hotkey",
            )?;
        }
    }

    if let Some(next_hotkey) = desired.window_hotkey.as_ref() {
        let normalized = next_hotkey.to_ascii_lowercase();
        if registered.insert(normalized) {
            register_hotkey(
                app,
                next_hotkey,
                CaptureEntryKind::Screenshot,
                Some(SettingsCaptureMode::Window),
                "window screenshot hotkey",
            )?;
        }
    }

    if let Some(next_hotkey) = desired.screenshot_hotkey.as_ref() {
        let normalized = next_hotkey.to_ascii_lowercase();
        if registered.insert(normalized) {
            register_hotkey(
                app,
                next_hotkey,
                CaptureEntryKind::Screenshot,
                Some(SettingsCaptureMode::Region),
                "region screenshot hotkey",
            )?;
        }
    }

    *guard = desired;
    Ok(())
}

fn capture_mode_to_str(mode: SettingsCaptureMode) -> &'static str {
    match mode {
        SettingsCaptureMode::Region => "region",
        SettingsCaptureMode::Fullscreen => "fullscreen",
        SettingsCaptureMode::Window => "window",
    }
}

fn register_hotkey(
    app: &AppHandle,
    hotkey: &str,
    entry_kind: CaptureEntryKind,
    initial_mode: Option<SettingsCaptureMode>,
    label: &str,
) -> Result<(), String> {
    app.global_shortcut()
        .on_shortcut(hotkey, move |app_handle, _shortcut, event| {
            if event.state != ShortcutState::Pressed {
                return;
            }

            if let Err(error) = open_capture_overlay_with_entry(app_handle, entry_kind, initial_mode)
            {
                eprintln!("failed to open OCR capture overlay: {error}");
            }
        })
        .map_err(|error| format!("failed to register {label} `{hotkey}`: {error}"))
}

pub fn open_capture_overlay(app: &AppHandle) -> Result<(), String> {
    open_capture_overlay_with_entry(
        app,
        CaptureEntryKind::Screenshot,
        Some(SettingsCaptureMode::Region),
    )
}

fn open_capture_overlay_with_entry(
    app: &AppHandle,
    entry_kind: CaptureEntryKind,
    initial_mode: Option<SettingsCaptureMode>,
) -> Result<(), String> {
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

    let _ = app.emit_to(
        WindowKind::OcrCapture.label(),
        "ocr-capture-opened",
        CaptureOpenedPayload {
            entry_kind: entry_kind.as_str(),
            initial_mode: initial_mode.map(capture_mode_to_str),
        },
    );

    let _ = window.unminimize();
    let _ = window.set_focus();

    Ok(())
}

pub fn capture_screenshot_preview(
    app: &AppHandle,
    request: ScreenshotCaptureRequest,
) -> Result<ScreenshotPreviewPayload, String> {
    let (_, monitor_position, monitor_size, monitor_scale) =
        capture_window_and_monitor(app).map_err(|error| error.to_string())?;

    let mut physical_rect = match request.mode.as_str() {
        "region" => {
            let region = request.region.ok_or_else(|| OcrError::InvalidRegion.to_string())?;
            logical_region_to_physical_from_monitor(
                monitor_position,
                monitor_size,
                monitor_scale,
                &region,
            )
                .map_err(|error| error.to_string())?
        }
        "fullscreen" => PhysicalCaptureRect {
            x: monitor_position.x,
            y: monitor_position.y,
            width: monitor_size.width,
            height: monitor_size.height,
        },
        "window" => {
            let point = request.point.ok_or_else(|| OcrError::InvalidRegion.to_string())?;
            let scale = if point.scale_factor.is_finite() && point.scale_factor > 0.0 {
                point.scale_factor
            } else {
                monitor_scale
            };
            let physical_x = monitor_position.x + (point.x.max(0.0) * scale).round() as i32;
            let physical_y = monitor_position.y + (point.y.max(0.0) * scale).round() as i32;

            resolve_window_rect_at_point(physical_x, physical_y).ok_or_else(|| OcrError::WindowNotFound.to_string())?
        }
        other => return Err(OcrError::UnsupportedMode(other.to_owned()).to_string()),
    };

    physical_rect = clamp_rect_to_monitor(physical_rect, monitor_position, monitor_size);
    if physical_rect.width == 0 || physical_rect.height == 0 {
        return Err(OcrError::InvalidRegion.to_string());
    }

    let data_url = capture_region_data_url(physical_rect).map_err(|error| error.to_string())?;
    let logical_rect = physical_to_logical_rect(physical_rect, monitor_position, monitor_scale);

    Ok(ScreenshotPreviewPayload {
        data_url,
        logical_rect,
    })
}

pub fn resolve_window_capture_hint(
    app: &AppHandle,
    point: CapturePoint,
) -> Result<Option<LogicalRectPayload>, String> {
    let (_, monitor_position, monitor_size, monitor_scale) =
        capture_window_and_monitor(app).map_err(|error| error.to_string())?;

    let scale = if point.scale_factor.is_finite() && point.scale_factor > 0.0 {
        point.scale_factor
    } else {
        monitor_scale
    };

    let physical_x = monitor_position.x + (point.x.max(0.0) * scale).round() as i32;
    let physical_y = monitor_position.y + (point.y.max(0.0) * scale).round() as i32;

    let Some(window_rect) = resolve_window_rect_at_point(physical_x, physical_y) else {
        return Ok(None);
    };

    let clamped = clamp_rect_to_monitor(window_rect, monitor_position, monitor_size);
    if clamped.width == 0 || clamped.height == 0 {
        return Ok(None);
    }

    Ok(Some(physical_to_logical_rect(
        clamped,
        monitor_position,
        monitor_scale,
    )))
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

    let (feature_width, feature_height) = resolve_feature_window_size(settings.window.window_size);
    manager::resize_window(app, route.target_window, feature_width, feature_height)
        .map_err(|error| OcrError::CaptureFailure(error.to_string()))?;

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
    let monitor = window
        .current_monitor()
        .map_err(|error| OcrError::CaptureFailure(error.to_string()))?
        .ok_or(OcrError::MonitorUnavailable)?;

    logical_region_to_physical_from_monitor(
        *monitor.position(),
        *monitor.size(),
        monitor.scale_factor(),
        region,
    )
}

fn logical_region_to_physical_from_monitor(
    monitor_position: PhysicalPosition<i32>,
    monitor_size: PhysicalSize<u32>,
    monitor_scale: f64,
    region: &OcrCaptureRegion,
) -> Result<PhysicalCaptureRect, OcrError> {
    if !region.width.is_finite()
        || !region.height.is_finite()
        || region.width <= 0.0
        || region.height <= 0.0
    {
        return Err(OcrError::InvalidRegion);
    }

    let scale = if region.scale_factor.is_finite() && region.scale_factor > 0.0 {
        region.scale_factor
    } else {
        monitor_scale
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

    let timeout_ms = ocr.timeout_ms.clamp(1_000, 120_000);
    match ocr.provider {
        OcrProvider::OpenaiVision => request_openai_ocr_text(ocr, image_data_url, timeout_ms).await,
        OcrProvider::GlmOcr => request_glm_ocr_text(ocr, image_data_url, timeout_ms).await,
    }
}

async fn request_openai_ocr_text(
    ocr: &OcrSettings,
    image_data_url: &str,
    timeout_ms: u64,
) -> Result<String, OcrError> {
    if ocr.model.trim().is_empty() {
        return Err(OcrError::EmptyModel);
    }

    let mut user_content = Vec::with_capacity(2);
    let prompt = ocr.prompt.trim();
    if !prompt.is_empty() {
        user_content.push(VisionContent::Text {
            text: prompt.to_owned(),
        });
    }

    user_content.push(VisionContent::ImageUrl {
        image_url: VisionImageUrl {
            url: image_data_url.to_owned(),
        },
    });

    let request_body = VisionChatRequest {
        model: ocr.model.trim().to_owned(),
        temperature: 0.0,
        messages: vec![VisionMessage {
            role: "user",
            content: user_content,
        }],
    };

    let client = reqwest::Client::builder()
        .timeout(std::time::Duration::from_millis(timeout_ms))
        .build()?;

    let response = client
        .post(build_openai_chat_url(ocr))
        .header(CONTENT_TYPE, "application/json")
        .header(AUTHORIZATION, format!("Bearer {}", ocr.api_key.trim()))
        .json(&request_body)
        .send()
        .await?
        .error_for_status()?
        .json::<serde_json::Value>()
        .await?;

    let text = extract_ocr_text_from_openai_response(&response).ok_or(OcrError::EmptyResponse)?;
    let trimmed = text.trim();
    if trimmed.is_empty() {
        return Err(OcrError::EmptyResponse);
    }

    Ok(trimmed.to_owned())
}

async fn request_glm_ocr_text(
    ocr: &OcrSettings,
    image_data_url: &str,
    timeout_ms: u64,
) -> Result<String, OcrError> {
    let client = reqwest::Client::builder()
        .timeout(std::time::Duration::from_millis(timeout_ms))
        .build()?;

    let response = client
        .post(GLM_OCR_LAYOUT_URL)
        .header(CONTENT_TYPE, "application/json")
        .header(AUTHORIZATION, ocr.api_key.trim())
        .json(&GlmLayoutRequest {
            model: GLM_OCR_FIXED_MODEL,
            file: image_data_url.to_owned(),
        })
        .send()
        .await?
        .error_for_status()?
        .json::<serde_json::Value>()
        .await?;

    let text = extract_text_from_glm_layout_response(&response).ok_or(OcrError::EmptyResponse)?;
    let trimmed = text.trim();
    if trimmed.is_empty() {
        return Err(OcrError::EmptyResponse);
    }

    Ok(trimmed.to_owned())
}

fn build_openai_chat_url(ocr: &OcrSettings) -> String {
    let trimmed = ocr.base_url.trim().trim_end_matches('/');
    let base = if trimmed.is_empty() {
        "https://api.openai.com/v1"
    } else {
        trimmed
    };

    if base.ends_with("/chat/completions") {
        return base.to_owned();
    }

    format!("{base}/chat/completions")
}

fn extract_ocr_text_from_openai_response(payload: &serde_json::Value) -> Option<String> {
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

fn extract_text_from_glm_layout_response(payload: &serde_json::Value) -> Option<String> {
    if let Some(text) = payload.get("text").and_then(|value| value.as_str()) {
        if !text.trim().is_empty() {
            return Some(text.trim().to_owned());
        }
    }

    let mut fragments = Vec::new();
    collect_text_fragments(payload, &mut fragments);
    if !fragments.is_empty() {
        let merged = fragments.join("\n");
        if !merged.trim().is_empty() {
            return Some(merged.trim().to_owned());
        }
    }

    extract_ocr_text_from_openai_response(payload)
}

fn collect_text_fragments(value: &serde_json::Value, fragments: &mut Vec<String>) {
    match value {
        serde_json::Value::Object(map) => {
            for (key, child) in map {
                if matches!(
                    key.as_str(),
                    "text" | "content" | "ocr_text" | "result_text" | "line_text" | "word"
                ) {
                    if let Some(text) = child.as_str() {
                        let trimmed = text.trim();
                        if !trimmed.is_empty() {
                            fragments.push(trimmed.to_owned());
                            continue;
                        }
                    }
                }

                collect_text_fragments(child, fragments);
            }
        }
        serde_json::Value::Array(items) => {
            for item in items {
                collect_text_fragments(item, fragments);
            }
        }
        _ => {}
    }
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

fn resolve_feature_window_size(preset: WindowSizePreset) -> (f64, f64) {
    match preset {
        WindowSizePreset::Large => (520.0, 400.0),
        WindowSizePreset::Medium => (460.0, 360.0),
        WindowSizePreset::Small => (400.0, 320.0),
    }
}

fn capture_window_and_monitor(
    app: &AppHandle,
) -> Result<(tauri::WebviewWindow, PhysicalPosition<i32>, PhysicalSize<u32>, f64), OcrError> {
    let capture_window = app
        .get_webview_window(WindowKind::OcrCapture.label())
        .ok_or(OcrError::CaptureWindowUnavailable)?;

    let monitor = capture_window
        .current_monitor()
        .map_err(|error| OcrError::CaptureFailure(error.to_string()))?
        .ok_or(OcrError::MonitorUnavailable)?;

    Ok((
        capture_window,
        *monitor.position(),
        *monitor.size(),
        monitor.scale_factor(),
    ))
}

fn clamp_rect_to_monitor(
    rect: PhysicalCaptureRect,
    monitor_position: PhysicalPosition<i32>,
    monitor_size: PhysicalSize<u32>,
) -> PhysicalCaptureRect {
    let monitor_left = monitor_position.x;
    let monitor_top = monitor_position.y;
    let monitor_right = monitor_left + i32::try_from(monitor_size.width).unwrap_or(i32::MAX);
    let monitor_bottom = monitor_top + i32::try_from(monitor_size.height).unwrap_or(i32::MAX);

    let rect_left = rect.x.max(monitor_left);
    let rect_top = rect.y.max(monitor_top);
    let rect_right = (rect.x + i32::try_from(rect.width).unwrap_or(i32::MAX)).min(monitor_right);
    let rect_bottom = (rect.y + i32::try_from(rect.height).unwrap_or(i32::MAX)).min(monitor_bottom);

    let width = rect_right.saturating_sub(rect_left).max(0) as u32;
    let height = rect_bottom.saturating_sub(rect_top).max(0) as u32;

    PhysicalCaptureRect {
        x: rect_left,
        y: rect_top,
        width,
        height,
    }
}

fn physical_to_logical_rect(
    rect: PhysicalCaptureRect,
    monitor_position: PhysicalPosition<i32>,
    monitor_scale: f64,
) -> LogicalRectPayload {
    let scale = if monitor_scale.is_finite() && monitor_scale > 0.0 {
        monitor_scale
    } else {
        1.0
    };

    LogicalRectPayload {
        x: f64::from(rect.x - monitor_position.x) / scale,
        y: f64::from(rect.y - monitor_position.y) / scale,
        width: f64::from(rect.width) / scale,
        height: f64::from(rect.height) / scale,
    }
}

#[cfg(target_os = "windows")]
fn resolve_window_rect_at_point(x: i32, y: i32) -> Option<PhysicalCaptureRect> {
    use std::ptr;

    use windows_sys::Win32::Foundation::{POINT, RECT};
    use windows_sys::Win32::System::Threading::GetCurrentProcessId;
    use windows_sys::Win32::UI::WindowsAndMessaging::{
        GetAncestor, GetWindow, GetWindowRect, GetWindowThreadProcessId, IsIconic, IsWindowVisible,
        WindowFromPoint, GA_ROOT, GW_HWNDNEXT,
    };

    unsafe {
        let mut hwnd = WindowFromPoint(POINT { x, y });
        let current_pid = GetCurrentProcessId();

        while !hwnd.is_null() {
            let mut pid = 0;
            GetWindowThreadProcessId(hwnd, &mut pid);

            let root = {
                let candidate = GetAncestor(hwnd, GA_ROOT);
                if !candidate.is_null() {
                    candidate
                } else {
                    hwnd
                }
            };

            if pid != current_pid && IsWindowVisible(root) != 0 && IsIconic(root) == 0 {
                let mut rect = RECT {
                    left: 0,
                    top: 0,
                    right: 0,
                    bottom: 0,
                };
                if GetWindowRect(root, &mut rect) != 0 {
                    let width = rect.right.saturating_sub(rect.left);
                    let height = rect.bottom.saturating_sub(rect.top);

                    if width > 2 && height > 2 {
                        return Some(PhysicalCaptureRect {
                            x: rect.left,
                            y: rect.top,
                            width: width as u32,
                            height: height as u32,
                        });
                    }
                }
            }

            hwnd = GetWindow(hwnd, GW_HWNDNEXT);
            if hwnd == ptr::null_mut() {
                break;
            }
        }
    }

    None
}

#[cfg(not(target_os = "windows"))]
fn resolve_window_rect_at_point(_x: i32, _y: i32) -> Option<PhysicalCaptureRect> {
    None
}

fn current_request_id() -> u64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map(|duration| duration.as_millis() as u64)
        .unwrap_or(0)
}
