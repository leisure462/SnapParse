#[cfg(target_os = "windows")]
use std::os::windows::process::CommandExt;
use std::{
    borrow::Cow,
    collections::{HashSet, VecDeque},
    fs,
    hash::{Hash, Hasher},
    io::{Cursor, ErrorKind},
    path::{Path, PathBuf},
    process::{Command, Stdio},
    sync::{
        atomic::{AtomicBool, AtomicIsize, AtomicU64, Ordering},
        Mutex,
    },
    time::{Duration, SystemTime, UNIX_EPOCH},
};

use arboard::{Clipboard, ImageData};
use base64::{engine::general_purpose::STANDARD as BASE64, Engine as _};
use chrono::{DateTime, Utc};
use image::{DynamicImage, ImageFormat, RgbaImage};
use reqwest::header::{AUTHORIZATION, CONTENT_TYPE};
use rfd::FileDialog;
use screenshots::Screen;
use serde::{Deserialize, Serialize};
use serde_json::json;
use tauri::{
    menu::MenuBuilder,
    tray::{MouseButton, MouseButtonState, TrayIconBuilder, TrayIconEvent},
    window::Color,
    AppHandle, Emitter, LogicalSize, Manager, PhysicalPosition, Position, Runtime, Size, State,
    WindowEvent,
};
use tauri_plugin_autostart::AutoLaunchManager;
use tauri_plugin_global_shortcut::{GlobalShortcutExt, ShortcutState};
use thiserror::Error;
#[cfg(target_os = "windows")]
use windows_sys::Win32::Foundation::{CloseHandle, HWND, POINT, RECT};
#[cfg(target_os = "windows")]
use windows_sys::Win32::Graphics::Dwm::{
    DwmSetWindowAttribute, DWMWA_WINDOW_CORNER_PREFERENCE, DWMWCP_ROUND,
};
#[cfg(target_os = "windows")]
use windows_sys::Win32::System::DataExchange::GetClipboardSequenceNumber;
#[cfg(target_os = "windows")]
use windows_sys::Win32::System::Threading::{
    OpenProcess, QueryFullProcessImageNameW, PROCESS_QUERY_LIMITED_INFORMATION,
};
#[cfg(target_os = "windows")]
use windows_sys::Win32::UI::Input::KeyboardAndMouse::{
    GetAsyncKeyState, SendInput, INPUT, INPUT_0, INPUT_KEYBOARD, KEYBDINPUT, KEYEVENTF_KEYUP,
    VK_CONTROL, VK_ESCAPE, VK_INSERT, VK_LBUTTON, VK_SHIFT,
};
#[cfg(target_os = "windows")]
use windows_sys::Win32::UI::WindowsAndMessaging::{
    GetClassNameW, GetCursorPos, GetForegroundWindow, GetWindowRect, GetWindowThreadProcessId,
    IsIconic, IsWindow, SetForegroundWindow, ShowWindow, SW_RESTORE,
};

const SETTINGS_VERSION: u32 = 9;
const WINDOW_LAYOUT_MIGRATION_VERSION: u32 = 7;
const DEFAULT_TOGGLE_SHORTCUT: &str = "Alt+Space";
const DEFAULT_TOGGLE_OCR_SHORTCUT: &str = "Alt+Shift+Space";
const SETTINGS_FILENAME: &str = "settings.json";
const SETTINGS_BACKUP_FILENAME: &str = "settings.bak.json";
const HISTORY_FILENAME: &str = "clipboard_history.json";
const SETTINGS_UPDATED_EVENT: &str = "snapparse://settings-updated";
const SETTINGS_WINDOW_SHOWN_EVENT: &str = "snapparse://settings-window-shown";
const MAIN_WINDOW_SHOWN_EVENT: &str = "snapparse://main-window-shown";
const SELECTION_DETECTED_EVENT: &str = "snapparse://selection-detected";
const SELECTION_RESULT_UPDATED_EVENT: &str = "snapparse://selection-result-updated";
const SELECTION_ERROR_EVENT: &str = "snapparse://selection-error";
const OCR_RESULT_UPDATED_EVENT: &str = "snapparse://ocr-result-updated";
const OCR_CAPTURE_STARTED_EVENT: &str = "snapparse://ocr-capture-started";
const OCR_CAPTURE_CANCELED_EVENT: &str = "snapparse://ocr-capture-canceled";
const OCR_ERROR_EVENT: &str = "snapparse://ocr-error";
const HISTORY_UPDATED_EVENT: &str = "snapparse://history-updated";

const MIN_POLL_MS: u64 = 400;
const MAX_POLL_MS: u64 = 5000;
const MIN_HISTORY_ITEMS: usize = 20;
const MAX_HISTORY_ITEMS: usize = 500;
const DEFAULT_MAIN_WINDOW_WIDTH: u32 = 350;
const DEFAULT_MAIN_WINDOW_HEIGHT: u32 = 520;
const MIN_MAIN_WINDOW_WIDTH: u32 = 248;
const MIN_MAIN_WINDOW_HEIGHT: u32 = 360;
const MAX_MAIN_WINDOW_WIDTH: u32 = 560;
const MAX_MAIN_WINDOW_HEIGHT: u32 = 980;
const DEFAULT_SELECTION_BAR_WIDTH: u32 = 330;
const SELECTION_BAR_HEIGHT: u32 = 44;
const DEFAULT_SELECTION_RESULT_WINDOW_WIDTH: u32 = 430;
const DEFAULT_SELECTION_RESULT_WINDOW_HEIGHT: u32 = 304;
const MIN_SELECTION_RESULT_WINDOW_WIDTH: u32 = 320;
const MIN_SELECTION_RESULT_WINDOW_HEIGHT: u32 = 220;
const MAX_SELECTION_RESULT_WINDOW_WIDTH: u32 = 980;
const MAX_SELECTION_RESULT_WINDOW_HEIGHT: u32 = 980;
const DEFAULT_OCR_RESULT_WINDOW_WIDTH: u32 = 360;
const DEFAULT_OCR_RESULT_WINDOW_HEIGHT: u32 = 480;
const MIN_OCR_RESULT_WINDOW_WIDTH: u32 = 320;
const MIN_OCR_RESULT_WINDOW_HEIGHT: u32 = 380;
const MAX_OCR_RESULT_WINDOW_WIDTH: u32 = 900;
const MAX_OCR_RESULT_WINDOW_HEIGHT: u32 = 1100;
const STREAM_EMIT_THROTTLE_MS: u64 = 24;
const DEFAULT_TTS_VOICE_ZH_CN: &str = "zh-CN-XiaoxiaoNeural";
const DEFAULT_TTS_VOICE_EN_US: &str = "en-US-JennyNeural";
const DEFAULT_SELECTION_BAR_AUTO_HIDE_MS: u64 = 5_000;
const MIN_TTS_RATE_PERCENT: i32 = -50;
const MAX_TTS_RATE_PERCENT: i32 = 100;
const MAX_TTS_TEXT_CHARS: usize = 12_000;
const TASK_REPLACED_ERROR: &str = "__TASK_REPLACED__";
const SELECTION_REPEAT_DEDUPE_WINDOW_MS: u64 = 900;
const SELECTION_TEXT_COOLDOWN_MS: u64 = 2_500;
const OCR_CAPTURE_BLUR_SUPPRESS_MS: u64 = 1_100;
const MODEL_REQUEST_MAX_ATTEMPTS: usize = 3;
const MODEL_REQUEST_RETRY_BASE_DELAY_MS: u64 = 140;
const MODEL_REQUEST_RETRY_MAX_DELAY_MS: u64 = 850;
const GLM_OCR_TEST_IMAGE_URL: &str = "https://cdn.bigmodel.cn/static/logo/introduction.png";
static EDGE_TTS_AUTO_INSTALL_ATTEMPTED: AtomicBool = AtomicBool::new(false);
const EDGE_TTS_INSTALL_IN_PROGRESS: &str = "__EDGE_TTS_INSTALL_IN_PROGRESS__";
#[cfg(target_os = "windows")]
const CREATE_NO_WINDOW: u32 = 0x08000000;

const MAIN_WINDOW_LABEL: &str = "main";
const SETTINGS_WINDOW_LABEL: &str = "settings";
const SELECTION_BAR_WINDOW_LABEL: &str = "selection_bar";
const SELECTION_RESULT_WINDOW_LABEL: &str = "selection_result";
const OCR_CAPTURE_WINDOW_LABEL: &str = "ocr_capture";
const OCR_RESULT_WINDOW_LABEL: &str = "ocr_result";
const TRAY_ID: &str = "snapparse-tray";
const TRAY_MENU_MAIN_ID: &str = "tray-open-main";
const TRAY_MENU_OCR_ID: &str = "tray-open-ocr";
const TRAY_MENU_SETTINGS_ID: &str = "tray-open-settings";
const TRAY_MENU_QUIT_ID: &str = "tray-quit";
const AUTOSTART_ARG: &str = "--autostart";
const BUILTIN_SELECTION_BAR_KEYS: [&str; 6] = [
    "copy",
    "summarize",
    "polish",
    "explain",
    "translate",
    "search",
];
const MAX_ENABLED_SELECTION_BAR_ITEMS: usize = 8;

#[derive(Clone, Copy, Serialize, Deserialize, PartialEq, Eq, Hash)]
#[serde(rename_all = "lowercase")]
enum ClipboardKind {
    Text,
    Link,
    Image,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq, Eq, Default)]
#[serde(rename_all = "lowercase")]
enum FilterKind {
    #[default]
    All,
    Text,
    Link,
    Image,
    Favorite,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq, Eq, Default)]
#[serde(rename_all = "kebab-case")]
enum DefaultOpenCategory {
    #[default]
    All,
    Text,
    Link,
    Image,
    Favorite,
    LastUsed,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq, Eq, Default)]
enum ThemePreset {
    #[serde(rename = "blue", alias = "md2-dark", alias = "midnight")]
    Blue,
    #[default]
    #[serde(rename = "deep-black", alias = "black", alias = "dark")]
    DeepBlack,
    #[serde(rename = "gray", alias = "graphite")]
    Gray,
    #[serde(
        rename = "white",
        alias = "daylight",
        alias = "sunrise",
        alias = "amber-mist"
    )]
    White,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq, Eq, Default)]
#[serde(rename_all = "kebab-case")]
enum PasteBehavior {
    CopyOnly,
    #[default]
    CopyAndHide,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq, Eq, Default)]
#[serde(rename_all = "kebab-case")]
enum SelectionTriggerMode {
    #[default]
    AutoDetect,
    CopyTrigger,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq, Eq, Default)]
#[serde(rename_all = "kebab-case")]
enum OcrDefaultAction {
    #[default]
    Translate,
    Summarize,
    Polish,
    Explain,
    Custom,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq, Eq, Default)]
#[serde(rename_all = "kebab-case")]
enum TtsRuntimeMode {
    #[default]
    DualFallback,
    EdgeCliOnly,
    PythonModuleOnly,
}

#[derive(Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", default)]
struct SelectionAssistantSettings {
    enabled: bool,
    mode: SelectionTriggerMode,
    show_icon_animation: bool,
    compact_mode: bool,
    auto_hide_ms: u64,
    search_url_template: String,
    min_chars: usize,
    max_chars: usize,
    blocked_apps: Vec<String>,
    default_translate_to: String,
    result_window_always_on_top: bool,
    remember_result_window_position: bool,
}

impl Default for SelectionAssistantSettings {
    fn default() -> Self {
        Self {
            enabled: true,
            mode: SelectionTriggerMode::AutoDetect,
            show_icon_animation: true,
            compact_mode: false,
            auto_hide_ms: DEFAULT_SELECTION_BAR_AUTO_HIDE_MS,
            search_url_template: "https://www.google.com/search?q={query}".to_string(),
            min_chars: 2,
            max_chars: 12_000,
            blocked_apps: Vec::new(),
            default_translate_to: "en-US".to_string(),
            result_window_always_on_top: true,
            remember_result_window_position: true,
        }
    }
}

#[derive(Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", default)]
struct LlmSettings {
    enabled: bool,
    base_url: String,
    api_key: String,
    model: String,
    temperature: f32,
    max_tokens: u32,
    timeout_ms: u64,
}

impl Default for LlmSettings {
    fn default() -> Self {
        Self {
            enabled: true,
            base_url: "https://api.openai.com/v1/chat/completions".to_string(),
            api_key: String::new(),
            model: "gpt-4o-mini".to_string(),
            temperature: 0.3,
            max_tokens: 1024,
            timeout_ms: 30_000,
        }
    }
}

#[derive(Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", default)]
struct VisionSettings {
    enabled: bool,
    base_url: String,
    api_key: String,
    model: String,
    temperature: f32,
    max_tokens: u32,
    timeout_ms: u64,
}

impl Default for VisionSettings {
    fn default() -> Self {
        Self {
            enabled: false,
            base_url: "https://api.openai.com/v1/chat/completions".to_string(),
            api_key: String::new(),
            model: "gpt-4o-mini".to_string(),
            temperature: 0.0,
            max_tokens: 2048,
            timeout_ms: 30_000,
        }
    }
}

#[derive(Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", default)]
struct OcrSettings {
    enabled: bool,
    auto_run_after_capture: bool,
    default_action: OcrDefaultAction,
    custom_agent_id: String,
    result_window_always_on_top: bool,
    remember_result_window_position: bool,
    vision: VisionSettings,
}

impl Default for OcrSettings {
    fn default() -> Self {
        Self {
            enabled: true,
            auto_run_after_capture: true,
            default_action: OcrDefaultAction::Translate,
            custom_agent_id: String::new(),
            result_window_always_on_top: true,
            remember_result_window_position: true,
            vision: VisionSettings::default(),
        }
    }
}

#[derive(Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", default)]
struct TtsSettings {
    runtime_mode: TtsRuntimeMode,
    voice_zh_cn: String,
    voice_en_us: String,
    rate_percent: i32,
}

impl Default for TtsSettings {
    fn default() -> Self {
        Self {
            runtime_mode: TtsRuntimeMode::DualFallback,
            voice_zh_cn: DEFAULT_TTS_VOICE_ZH_CN.to_string(),
            voice_en_us: DEFAULT_TTS_VOICE_EN_US.to_string(),
            rate_percent: 0,
        }
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", default)]
struct CustomAgent {
    id: String,
    name: String,
    icon: String,
    prompt: String,
    enabled: bool,
    order: u32,
}

impl Default for CustomAgent {
    fn default() -> Self {
        Self {
            id: String::new(),
            name: String::new(),
            icon: "Sparkles".to_string(),
            prompt: String::new(),
            enabled: true,
            order: 0,
        }
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", default)]
struct SelectionBarItemConfig {
    key: String,
    enabled: bool,
    order: u32,
}

impl Default for SelectionBarItemConfig {
    fn default() -> Self {
        Self {
            key: String::new(),
            enabled: true,
            order: 0,
        }
    }
}

#[derive(Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", default)]
struct AgentSettings {
    custom: Vec<CustomAgent>,
    bar_order: Vec<SelectionBarItemConfig>,
}

impl Default for AgentSettings {
    fn default() -> Self {
        Self {
            custom: Vec::new(),
            bar_order: builtin_selection_bar_order(),
        }
    }
}

#[derive(Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", default)]
struct WindowSettings {
    auto_hide_on_blur: bool,
    remember_position: bool,
    remember_main_window_size: bool,
    launch_on_system_startup: bool,
    silent_startup: bool,
    check_updates_on_startup: bool,
}

impl Default for WindowSettings {
    fn default() -> Self {
        Self {
            auto_hide_on_blur: true,
            remember_position: true,
            remember_main_window_size: true,
            launch_on_system_startup: false,
            silent_startup: false,
            check_updates_on_startup: false,
        }
    }
}

#[derive(Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", default)]
struct ShortcutSettings {
    toggle_main: String,
    toggle_ocr: String,
}

impl Default for ShortcutSettings {
    fn default() -> Self {
        Self {
            toggle_main: DEFAULT_TOGGLE_SHORTCUT.to_string(),
            toggle_ocr: DEFAULT_TOGGLE_OCR_SHORTCUT.to_string(),
        }
    }
}

#[derive(Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", default)]
struct HistorySettings {
    poll_ms: u64,
    max_items: usize,
    dedupe: bool,
    capture_text: bool,
    capture_link: bool,
    capture_image: bool,
    default_open_category: DefaultOpenCategory,
    default_category: FilterKind,
    paste_behavior: PasteBehavior,
    collapse_top_bar: bool,
    promote_after_paste: bool,
    open_at_top_on_show: bool,
    storage_path: String,
}

impl Default for HistorySettings {
    fn default() -> Self {
        Self {
            poll_ms: 1200,
            max_items: 120,
            dedupe: true,
            capture_text: true,
            capture_link: true,
            capture_image: true,
            default_open_category: DefaultOpenCategory::All,
            default_category: FilterKind::All,
            paste_behavior: PasteBehavior::CopyAndHide,
            collapse_top_bar: false,
            promote_after_paste: true,
            open_at_top_on_show: true,
            storage_path: String::new(),
        }
    }
}

#[derive(Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", default)]
struct AppSettings {
    version: u32,
    theme_preset: ThemePreset,
    language: String,
    window: WindowSettings,
    selection_assistant: SelectionAssistantSettings,
    llm: LlmSettings,
    tts: TtsSettings,
    agents: AgentSettings,
    shortcuts: ShortcutSettings,
    ocr: OcrSettings,
    history: HistorySettings,
    main_window_width: Option<u32>,
    main_window_height: Option<u32>,
    main_window_x: Option<i32>,
    main_window_y: Option<i32>,
    selection_result_window_width: Option<u32>,
    selection_result_window_height: Option<u32>,
    selection_result_window_x: Option<i32>,
    selection_result_window_y: Option<i32>,
    ocr_result_window_width: Option<u32>,
    ocr_result_window_height: Option<u32>,
    ocr_result_window_x: Option<i32>,
    ocr_result_window_y: Option<i32>,
}

impl Default for AppSettings {
    fn default() -> Self {
        Self {
            version: SETTINGS_VERSION,
            theme_preset: ThemePreset::DeepBlack,
            language: "zh-CN".to_string(),
            window: WindowSettings::default(),
            selection_assistant: SelectionAssistantSettings::default(),
            llm: LlmSettings::default(),
            tts: TtsSettings::default(),
            agents: AgentSettings::default(),
            shortcuts: ShortcutSettings::default(),
            ocr: OcrSettings::default(),
            history: HistorySettings::default(),
            main_window_width: Some(DEFAULT_MAIN_WINDOW_WIDTH),
            main_window_height: Some(DEFAULT_MAIN_WINDOW_HEIGHT),
            main_window_x: None,
            main_window_y: None,
            selection_result_window_width: Some(DEFAULT_SELECTION_RESULT_WINDOW_WIDTH),
            selection_result_window_height: Some(DEFAULT_SELECTION_RESULT_WINDOW_HEIGHT),
            selection_result_window_x: None,
            selection_result_window_y: None,
            ocr_result_window_width: Some(DEFAULT_OCR_RESULT_WINDOW_WIDTH),
            ocr_result_window_height: Some(DEFAULT_OCR_RESULT_WINDOW_HEIGHT),
            ocr_result_window_x: None,
            ocr_result_window_y: None,
        }
    }
}
#[derive(Debug, Deserialize, Default)]
#[serde(rename_all = "camelCase", default)]
struct WindowSettingsPatch {
    auto_hide_on_blur: Option<bool>,
    remember_position: Option<bool>,
    remember_main_window_size: Option<bool>,
    launch_on_system_startup: Option<bool>,
    silent_startup: Option<bool>,
    check_updates_on_startup: Option<bool>,
}

#[derive(Debug, Deserialize, Default)]
#[serde(rename_all = "camelCase", default)]
struct SelectionAssistantSettingsPatch {
    enabled: Option<bool>,
    mode: Option<SelectionTriggerMode>,
    show_icon_animation: Option<bool>,
    compact_mode: Option<bool>,
    auto_hide_ms: Option<u64>,
    search_url_template: Option<String>,
    min_chars: Option<usize>,
    max_chars: Option<usize>,
    blocked_apps: Option<Vec<String>>,
    default_translate_to: Option<String>,
    result_window_always_on_top: Option<bool>,
    remember_result_window_position: Option<bool>,
}

#[derive(Debug, Deserialize, Default)]
#[serde(rename_all = "camelCase", default)]
struct LlmSettingsPatch {
    enabled: Option<bool>,
    base_url: Option<String>,
    api_key: Option<String>,
    model: Option<String>,
    temperature: Option<f32>,
    max_tokens: Option<u32>,
    timeout_ms: Option<u64>,
}

#[derive(Debug, Deserialize, Default)]
#[serde(rename_all = "camelCase", default)]
struct TtsSettingsPatch {
    runtime_mode: Option<TtsRuntimeMode>,
    voice_zh_cn: Option<String>,
    voice_en_us: Option<String>,
    rate_percent: Option<i32>,
}

#[derive(Debug, Deserialize, Default)]
#[serde(rename_all = "camelCase", default)]
struct VisionSettingsPatch {
    enabled: Option<bool>,
    base_url: Option<String>,
    api_key: Option<String>,
    model: Option<String>,
    temperature: Option<f32>,
    max_tokens: Option<u32>,
    timeout_ms: Option<u64>,
}

#[derive(Debug, Deserialize, Default)]
#[serde(rename_all = "camelCase", default)]
struct OcrSettingsPatch {
    enabled: Option<bool>,
    auto_run_after_capture: Option<bool>,
    default_action: Option<OcrDefaultAction>,
    custom_agent_id: Option<String>,
    result_window_always_on_top: Option<bool>,
    remember_result_window_position: Option<bool>,
    vision: Option<VisionSettingsPatch>,
}

#[derive(Debug, Deserialize, Default)]
#[serde(rename_all = "camelCase", default)]
struct AgentSettingsPatch {
    custom: Option<Vec<CustomAgent>>,
    bar_order: Option<Vec<SelectionBarItemConfig>>,
}

#[derive(Debug, Deserialize, Default)]
#[serde(rename_all = "camelCase", default)]
struct ShortcutSettingsPatch {
    toggle_main: Option<String>,
    toggle_ocr: Option<String>,
}

#[derive(Debug, Deserialize, Default)]
#[serde(rename_all = "camelCase", default)]
struct HistorySettingsPatch {
    poll_ms: Option<u64>,
    max_items: Option<usize>,
    dedupe: Option<bool>,
    capture_text: Option<bool>,
    capture_link: Option<bool>,
    capture_image: Option<bool>,
    default_open_category: Option<DefaultOpenCategory>,
    default_category: Option<FilterKind>,
    paste_behavior: Option<PasteBehavior>,
    collapse_top_bar: Option<bool>,
    promote_after_paste: Option<bool>,
    open_at_top_on_show: Option<bool>,
    storage_path: Option<String>,
}

#[derive(Debug, Deserialize, Default)]
#[serde(rename_all = "camelCase", default)]
struct SettingsPatch {
    theme_preset: Option<ThemePreset>,
    language: Option<String>,
    window: Option<WindowSettingsPatch>,
    selection_assistant: Option<SelectionAssistantSettingsPatch>,
    llm: Option<LlmSettingsPatch>,
    tts: Option<TtsSettingsPatch>,
    agents: Option<AgentSettingsPatch>,
    shortcuts: Option<ShortcutSettingsPatch>,
    ocr: Option<OcrSettingsPatch>,
    history: Option<HistorySettingsPatch>,
    selection_result_window_width: Option<u32>,
    selection_result_window_height: Option<u32>,
    ocr_result_window_width: Option<u32>,
    ocr_result_window_height: Option<u32>,
}

#[derive(Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
struct ClipboardEntry {
    id: String,
    kind: ClipboardKind,
    content: String,
    image_data_url: Option<String>,
    copied_at: DateTime<Utc>,
    pinned: bool,
}

#[derive(Default)]
struct ClipboardState {
    history: VecDeque<ClipboardEntry>,
    last_observed_signature: Option<u64>,
    last_clipboard_sequence: Option<u32>,
}

#[derive(Default)]
struct RuntimeFlags {
    allow_exit: AtomicBool,
    main_window_pinned: AtomicBool,
    suppress_auto_hide_until_ms: AtomicU64,
    last_foreground_hwnd: AtomicIsize,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum SelectionDetectPhase {
    Idle,
    Dragging,
}

#[derive(Clone, Copy, Debug)]
struct SelectionDispatchMarker {
    text_hash: u64,
    source_hwnd: isize,
    mode: SelectionTriggerMode,
    emitted_at_ms: u64,
}

#[derive(Default)]
struct SelectionRuntimeState {
    detector_running: AtomicBool,
    last_dispatch_marker: Mutex<Option<SelectionDispatchMarker>>,
    last_clipboard_observed: Mutex<String>,
    active_result_request_nonce: AtomicU64,
}

struct OcrCaptureSnapshot {
    monitor_x: i32,
    monitor_y: i32,
    image: RgbaImage,
}

struct OcrRuntimeState {
    capture_active: AtomicBool,
    active_result_request_nonce: AtomicU64,
    suppress_blur_until_ms: AtomicU64,
    capture_snapshot: Mutex<Option<OcrCaptureSnapshot>>,
}

impl Default for OcrRuntimeState {
    fn default() -> Self {
        Self {
            capture_active: AtomicBool::new(false),
            active_result_request_nonce: AtomicU64::new(0),
            suppress_blur_until_ms: AtomicU64::new(0),
            capture_snapshot: Mutex::new(None),
        }
    }
}

struct AppSettingsState {
    file_path: PathBuf,
    data: Mutex<AppSettings>,
}

struct SettingsLoadResult {
    settings: AppSettings,
    source_path: PathBuf,
    should_persist: bool,
}

struct HistoryLoadResult {
    history: VecDeque<ClipboardEntry>,
    source_path: Option<PathBuf>,
    should_persist: bool,
}

struct HttpClientState {
    client: reqwest::Client,
}

impl Default for HttpClientState {
    fn default() -> Self {
        let client = reqwest::Client::builder()
            .pool_max_idle_per_host(32)
            .pool_idle_timeout(Duration::from_secs(90))
            .tcp_nodelay(true)
            .build()
            .unwrap_or_else(|_| reqwest::Client::new());
        Self { client }
    }
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
struct SelectionDetectedPayload {
    text: String,
    x: i32,
    y: i32,
    mode: SelectionTriggerMode,
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
struct MainWindowShownPayload {
    collapse_top_bar: bool,
    open_to_top: bool,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
struct SelectionBarOpenPayload {
    text: String,
    x: i32,
    y: i32,
    mode: SelectionTriggerMode,
}

#[derive(Clone, Copy, Debug, Deserialize)]
#[serde(rename_all = "kebab-case")]
enum SelectionActionKind {
    Summarize,
    Polish,
    Explain,
    Translate,
    Custom,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
struct RunSelectionActionPayload {
    action: SelectionActionKind,
    text: String,
    custom_agent_id: Option<String>,
    translate_from: Option<String>,
    translate_to: Option<String>,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
struct RunOcrActionPayload {
    action: SelectionActionKind,
    ocr_text: String,
    custom_agent_id: Option<String>,
    translate_from: Option<String>,
    translate_to: Option<String>,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
struct SynthesizeTtsPayload {
    text: String,
    language_hint: Option<String>,
    voice_override: Option<String>,
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
struct SelectionResultPayload {
    request_id: String,
    action: String,
    source_text: String,
    output_text: String,
    translate_from: Option<String>,
    translate_to: Option<String>,
    custom_agent_name: Option<String>,
    custom_agent_icon: Option<String>,
    is_streaming: bool,
    error_message: Option<String>,
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
struct OcrResultPayload {
    request_id: String,
    action: String,
    ocr_text: String,
    output_text: String,
    translate_from: Option<String>,
    translate_to: Option<String>,
    custom_agent_name: Option<String>,
    custom_agent_icon: Option<String>,
    is_streaming: bool,
    error_message: Option<String>,
}

#[derive(Clone, Serialize)]
#[serde(rename_all = "camelCase")]
struct SynthesizeTtsResult {
    audio_base64: String,
    mime_type: String,
    voice_used: String,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
struct OcrCaptureAreaPayload {
    x: f64,
    y: f64,
    width: f64,
    height: f64,
}

#[derive(Clone)]
struct ClipboardImageSnapshot {
    width: usize,
    height: usize,
    bytes: Vec<u8>,
}

#[derive(Clone)]
enum ClipboardSnapshot {
    Empty,
    Text(String),
    Image(ClipboardImageSnapshot),
}

#[derive(Debug, Error)]
enum CommandError {
    #[error("Clipboard access failed: {0}")]
    Clipboard(String),
    #[error("History item not found")]
    NotFound,
    #[error("State lock poisoned")]
    Lock,
    #[error("Image processing failed: {0}")]
    InvalidImage(String),
    #[error("Settings error: {0}")]
    Settings(String),
    #[error("Serialization error: {0}")]
    Serialization(String),
    #[error("Shortcut registration failed: {0}")]
    Shortcut(String),
}

impl Serialize for CommandError {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.serialize_str(self.to_string().as_ref())
    }
}

fn now_id() -> String {
    format!("{}", Utc::now().timestamp_nanos_opt().unwrap_or_default())
}

fn now_epoch_millis() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_millis() as u64)
        .unwrap_or(0)
}

fn should_retry_http_status(status: reqwest::StatusCode) -> bool {
    let code = status.as_u16();
    status.is_server_error() || matches!(code, 408 | 409 | 425 | 429)
}

fn should_retry_network_error(error: &reqwest::Error) -> bool {
    error.is_timeout() || error.is_connect() || error.is_request() || error.is_body()
}

fn retry_backoff_delay_ms(attempt: usize) -> u64 {
    let multiplier = 1u64 << attempt.min(4);
    MODEL_REQUEST_RETRY_BASE_DELAY_MS
        .saturating_mul(multiplier)
        .min(MODEL_REQUEST_RETRY_MAX_DELAY_MS)
}

async fn sleep_with_backoff(attempt: usize) {
    let delay = retry_backoff_delay_ms(attempt);
    if delay == 0 {
        return;
    }
    let _ = tauri::async_runtime::spawn_blocking(move || {
        std::thread::sleep(Duration::from_millis(delay));
    })
    .await;
}

async fn sleep_for_ms(ms: u64) {
    if ms == 0 {
        return;
    }
    let _ = tauri::async_runtime::spawn_blocking(move || {
        std::thread::sleep(Duration::from_millis(ms));
    })
    .await;
}

fn format_response_body_for_error(raw_body: &str) -> String {
    let trimmed = raw_body.trim();
    if trimmed.is_empty() {
        return "Empty response body".to_string();
    }

    const MAX_CHARS: usize = 1200;
    let mut output = trimmed.chars().take(MAX_CHARS).collect::<String>();
    if trimmed.chars().nth(MAX_CHARS).is_some() {
        output.push_str("...");
    }
    output
}

fn with_history_lock<'a>(
    state: &'a State<'_, Mutex<ClipboardState>>,
) -> Result<std::sync::MutexGuard<'a, ClipboardState>, CommandError> {
    state.lock().map_err(|_| CommandError::Lock)
}

fn with_settings_lock<'a>(
    state: &'a State<'_, AppSettingsState>,
) -> Result<std::sync::MutexGuard<'a, AppSettings>, CommandError> {
    state.data.lock().map_err(|_| CommandError::Lock)
}

fn clamp_poll_ms(value: u64) -> u64 {
    value.clamp(MIN_POLL_MS, MAX_POLL_MS)
}

fn clamp_history_items(value: usize) -> usize {
    value.clamp(MIN_HISTORY_ITEMS, MAX_HISTORY_ITEMS)
}

fn clamp_main_window_width(value: u32) -> u32 {
    value.clamp(MIN_MAIN_WINDOW_WIDTH, MAX_MAIN_WINDOW_WIDTH)
}

fn clamp_main_window_height(value: u32) -> u32 {
    value.clamp(MIN_MAIN_WINDOW_HEIGHT, MAX_MAIN_WINDOW_HEIGHT)
}

fn clamp_selection_result_window_width(value: u32) -> u32 {
    value.clamp(
        MIN_SELECTION_RESULT_WINDOW_WIDTH,
        MAX_SELECTION_RESULT_WINDOW_WIDTH,
    )
}

fn clamp_selection_result_window_height(value: u32) -> u32 {
    value.clamp(
        MIN_SELECTION_RESULT_WINDOW_HEIGHT,
        MAX_SELECTION_RESULT_WINDOW_HEIGHT,
    )
}

fn clamp_ocr_result_window_width(value: u32) -> u32 {
    value.clamp(MIN_OCR_RESULT_WINDOW_WIDTH, MAX_OCR_RESULT_WINDOW_WIDTH)
}

fn clamp_ocr_result_window_height(value: u32) -> u32 {
    value.clamp(MIN_OCR_RESULT_WINDOW_HEIGHT, MAX_OCR_RESULT_WINDOW_HEIGHT)
}

fn clamp_selection_auto_hide_ms(value: u64) -> u64 {
    value.clamp(800, 30_000)
}

fn clamp_selection_min_chars(value: usize) -> usize {
    value.clamp(1, 64)
}

fn clamp_selection_max_chars(value: usize) -> usize {
    value.clamp(128, 100_000)
}

fn to_i32(value: u32) -> i32 {
    value.min(i32::MAX as u32) as i32
}

fn monitor_bounds(monitor: &tauri::Monitor) -> (i32, i32, i32, i32) {
    let position = monitor.position();
    let size = monitor.size();
    let max_x = position.x.saturating_add(to_i32(size.width));
    let max_y = position.y.saturating_add(to_i32(size.height));
    (position.x, position.y, max_x, max_y)
}

fn monitor_contains_point(monitor: &tauri::Monitor, point: PhysicalPosition<i32>) -> bool {
    let (min_x, min_y, max_x, max_y) = monitor_bounds(monitor);
    point.x >= min_x && point.x < max_x && point.y >= min_y && point.y < max_y
}

fn clamp_position_to_monitor(
    monitor: &tauri::Monitor,
    candidate: PhysicalPosition<i32>,
    width: u32,
    height: u32,
) -> PhysicalPosition<i32> {
    let monitor_pos = monitor.position();
    let monitor_size = monitor.size();
    let scale_factor = monitor.scale_factor().max(0.1);

    // `width`/`height` are logical window sizes from settings/config.
    // Convert to physical pixels before clamping against monitor bounds.
    let window_width_px = ((width as f64) * scale_factor)
        .round()
        .clamp(1.0, i32::MAX as f64) as i32;
    let window_height_px = ((height as f64) * scale_factor)
        .round()
        .clamp(1.0, i32::MAX as f64) as i32;

    let min_x = monitor_pos.x;
    let min_y = monitor_pos.y;
    let max_x = min_x
        .saturating_add(to_i32(monitor_size.width))
        .saturating_sub(window_width_px)
        .max(min_x);
    let max_y = min_y
        .saturating_add(to_i32(monitor_size.height))
        .saturating_sub(window_height_px)
        .max(min_y);

    PhysicalPosition::new(
        candidate.x.clamp(min_x, max_x),
        candidate.y.clamp(min_y, max_y),
    )
}

fn monitor_for_point<R: Runtime>(
    app: &AppHandle<R>,
    point: PhysicalPosition<i32>,
) -> Option<tauri::Monitor> {
    let monitors = app.available_monitors().ok()?;
    monitors
        .into_iter()
        .find(|monitor| monitor_contains_point(monitor, point))
        .or_else(|| app.primary_monitor().ok().flatten())
}

#[cfg(target_os = "windows")]
fn current_pointer_position() -> Option<PhysicalPosition<i32>> {
    let mut point = POINT { x: 0, y: 0 };
    if unsafe { GetCursorPos(&mut point as *mut POINT) } != 0 {
        Some(PhysicalPosition::new(point.x, point.y))
    } else {
        None
    }
}

#[cfg(not(target_os = "windows"))]
fn current_pointer_position() -> Option<PhysicalPosition<i32>> {
    None
}

fn is_saved_size_near_monitor_bounds(
    width: u32,
    height: u32,
    monitor_width: u32,
    monitor_height: u32,
) -> bool {
    if monitor_width == 0 || monitor_height == 0 {
        return false;
    }

    let width_limit = (monitor_width as f64 * 0.92).round() as u32;
    let height_limit = (monitor_height as f64 * 0.92).round() as u32;
    width >= width_limit && height >= height_limit
}

fn normalize_shortcut(value: &str, fallback: &str) -> String {
    let trimmed = value.trim();
    if trimmed.is_empty() {
        fallback.to_string()
    } else {
        trimmed.to_string()
    }
}

fn normalize_language(value: &str) -> String {
    if value.eq_ignore_ascii_case("en-US") {
        "en-US".to_string()
    } else {
        "zh-CN".to_string()
    }
}

fn normalize_translate_language(value: &str, fallback: &str) -> String {
    let trimmed = value.trim();
    match trimmed {
        "zh-CN" | "en-US" | "ja-JP" | "ko-KR" => trimmed.to_string(),
        _ => fallback.to_string(),
    }
}

fn default_translate_target_for_language(language: &str) -> &'static str {
    if language.eq_ignore_ascii_case("en-US") {
        "zh-CN"
    } else {
        "en-US"
    }
}

fn normalize_tts_voice(value: &str, fallback: &str) -> String {
    let trimmed = value.trim();
    if trimmed.is_empty() {
        fallback.to_string()
    } else {
        trimmed.to_string()
    }
}

fn is_cjk_char(ch: char) -> bool {
    matches!(
        ch as u32,
        0x3400..=0x4DBF
            | 0x4E00..=0x9FFF
            | 0xF900..=0xFAFF
            | 0x20000..=0x2A6DF
            | 0x2A700..=0x2B73F
            | 0x2B740..=0x2B81F
            | 0x2B820..=0x2CEAF
    )
}

fn trim_name_by_units(value: &str, max_units: u32) -> String {
    let mut result = String::new();
    let mut used = 0u32;
    for ch in value.trim().chars() {
        let weight = if is_cjk_char(ch) { 2 } else { 1 };
        if used.saturating_add(weight) > max_units {
            break;
        }
        result.push(ch);
        used = used.saturating_add(weight);
    }
    result.trim().to_string()
}

fn is_builtin_selection_bar_key(value: &str) -> bool {
    BUILTIN_SELECTION_BAR_KEYS.contains(&value)
}

fn parse_custom_selection_bar_key(value: &str) -> Option<&str> {
    value
        .strip_prefix("custom:")
        .map(str::trim)
        .filter(|item| !item.is_empty())
}

fn builtin_selection_bar_order() -> Vec<SelectionBarItemConfig> {
    BUILTIN_SELECTION_BAR_KEYS
        .iter()
        .enumerate()
        .map(|(index, key)| SelectionBarItemConfig {
            key: (*key).to_string(),
            enabled: true,
            order: index as u32,
        })
        .collect()
}

fn normalize_selection_bar_order(
    input: Vec<SelectionBarItemConfig>,
    custom_agents: &[CustomAgent],
) -> Vec<SelectionBarItemConfig> {
    let custom_keys: HashSet<String> = custom_agents
        .iter()
        .map(|item| format!("custom:{}", item.id))
        .collect();
    let mut seen = HashSet::new();
    let mut normalized: Vec<SelectionBarItemConfig> = Vec::new();

    let mut sorted = input;
    sorted.sort_by_key(|item| item.order);

    for item in sorted {
        let key = item.key.trim().to_string();
        if key.is_empty() || seen.contains(&key) {
            continue;
        }
        let valid = is_builtin_selection_bar_key(&key)
            || parse_custom_selection_bar_key(&key)
                .map(|custom_id| custom_keys.contains(&format!("custom:{}", custom_id)))
                .unwrap_or(false);
        if !valid {
            continue;
        }
        seen.insert(key.clone());
        normalized.push(SelectionBarItemConfig {
            key,
            enabled: item.enabled,
            order: normalized.len() as u32,
        });
    }

    for key in BUILTIN_SELECTION_BAR_KEYS {
        if seen.contains(key) {
            continue;
        }
        seen.insert(key.to_string());
        normalized.push(SelectionBarItemConfig {
            key: key.to_string(),
            enabled: true,
            order: normalized.len() as u32,
        });
    }

    for agent in custom_agents {
        let key = format!("custom:{}", agent.id);
        if seen.contains(&key) {
            continue;
        }
        seen.insert(key.clone());
        normalized.push(SelectionBarItemConfig {
            key,
            enabled: agent.enabled,
            order: normalized.len() as u32,
        });
    }

    let mut enabled_count = 0usize;
    for item in normalized.iter_mut() {
        if !item.enabled {
            continue;
        }
        if enabled_count >= MAX_ENABLED_SELECTION_BAR_ITEMS {
            item.enabled = false;
            continue;
        }
        enabled_count += 1;
    }

    normalized
}

fn normalize_settings(settings: &mut AppSettings) {
    let previous_version = settings.version;
    settings.version = SETTINGS_VERSION;
    settings.language = normalize_language(&settings.language);
    settings.shortcuts.toggle_main =
        normalize_shortcut(&settings.shortcuts.toggle_main, DEFAULT_TOGGLE_SHORTCUT);
    settings.shortcuts.toggle_ocr =
        normalize_shortcut(&settings.shortcuts.toggle_ocr, DEFAULT_TOGGLE_OCR_SHORTCUT);
    settings.history.poll_ms = clamp_poll_ms(settings.history.poll_ms);
    settings.history.max_items = clamp_history_items(settings.history.max_items);
    settings.history.storage_path = settings.history.storage_path.trim().to_string();

    let mut width = settings
        .main_window_width
        .unwrap_or(DEFAULT_MAIN_WINDOW_WIDTH);
    let mut height = settings
        .main_window_height
        .unwrap_or(DEFAULT_MAIN_WINDOW_HEIGHT);
    if previous_version < WINDOW_LAYOUT_MIGRATION_VERSION
        || (width == 420 && height == 720)
        || (width == 380 && height == 620)
        || (width == 340 && height == 560)
        || width >= 900
        || height >= 1300
    {
        width = DEFAULT_MAIN_WINDOW_WIDTH;
        height = DEFAULT_MAIN_WINDOW_HEIGHT;
    }
    settings.main_window_width = Some(clamp_main_window_width(width));
    settings.main_window_height = Some(clamp_main_window_height(height));
    if previous_version < WINDOW_LAYOUT_MIGRATION_VERSION {
        settings.main_window_x = None;
        settings.main_window_y = None;
    }

    let mut selection_result_width = settings
        .selection_result_window_width
        .unwrap_or(DEFAULT_SELECTION_RESULT_WINDOW_WIDTH);
    let mut selection_result_height = settings
        .selection_result_window_height
        .unwrap_or(DEFAULT_SELECTION_RESULT_WINDOW_HEIGHT);
    if previous_version < WINDOW_LAYOUT_MIGRATION_VERSION {
        selection_result_width = DEFAULT_SELECTION_RESULT_WINDOW_WIDTH;
        selection_result_height = DEFAULT_SELECTION_RESULT_WINDOW_HEIGHT;
    }
    settings.selection_result_window_width =
        Some(clamp_selection_result_window_width(selection_result_width));
    settings.selection_result_window_height = Some(clamp_selection_result_window_height(
        selection_result_height,
    ));
    if previous_version < WINDOW_LAYOUT_MIGRATION_VERSION {
        settings.selection_result_window_x = None;
        settings.selection_result_window_y = None;
    }

    let mut ocr_result_width = settings
        .ocr_result_window_width
        .unwrap_or(DEFAULT_OCR_RESULT_WINDOW_WIDTH);
    let mut ocr_result_height = settings
        .ocr_result_window_height
        .unwrap_or(DEFAULT_OCR_RESULT_WINDOW_HEIGHT);
    if previous_version < WINDOW_LAYOUT_MIGRATION_VERSION {
        ocr_result_width = DEFAULT_OCR_RESULT_WINDOW_WIDTH;
        ocr_result_height = DEFAULT_OCR_RESULT_WINDOW_HEIGHT;
    } else if ocr_result_width == 380 && ocr_result_height == 520 {
        // Migrate legacy default OCR result size to the new, more compact default.
        ocr_result_width = DEFAULT_OCR_RESULT_WINDOW_WIDTH;
        ocr_result_height = DEFAULT_OCR_RESULT_WINDOW_HEIGHT;
    }
    settings.ocr_result_window_width = Some(clamp_ocr_result_window_width(ocr_result_width));
    settings.ocr_result_window_height = Some(clamp_ocr_result_window_height(ocr_result_height));
    if previous_version < WINDOW_LAYOUT_MIGRATION_VERSION {
        settings.ocr_result_window_x = None;
        settings.ocr_result_window_y = None;
    }

    if previous_version < SETTINGS_VERSION {
        settings.selection_assistant.enabled = true;
        settings.ocr.enabled = true;
        if previous_version < 9 {
            let legacy_remember_position = settings.window.remember_position;
            settings.selection_assistant.remember_result_window_position = legacy_remember_position;
            settings.ocr.remember_result_window_position = legacy_remember_position;
            settings.history.promote_after_paste = true;
            settings.history.open_at_top_on_show = true;
        }
    }

    if settings.selection_assistant.auto_hide_ms == 3600 {
        settings.selection_assistant.auto_hide_ms = DEFAULT_SELECTION_BAR_AUTO_HIDE_MS;
    }
    settings.selection_assistant.auto_hide_ms =
        clamp_selection_auto_hide_ms(settings.selection_assistant.auto_hide_ms);
    settings.selection_assistant.min_chars =
        clamp_selection_min_chars(settings.selection_assistant.min_chars);
    settings.selection_assistant.max_chars =
        clamp_selection_max_chars(settings.selection_assistant.max_chars);
    if settings.selection_assistant.max_chars < settings.selection_assistant.min_chars {
        settings.selection_assistant.max_chars = settings.selection_assistant.min_chars.max(128);
    }
    let search_template = settings.selection_assistant.search_url_template.trim();
    settings.selection_assistant.search_url_template = if search_template.is_empty() {
        "https://www.google.com/search?q={query}".to_string()
    } else {
        search_template.to_string()
    };
    let blocked_apps = std::mem::take(&mut settings.selection_assistant.blocked_apps);
    settings.selection_assistant.blocked_apps = blocked_apps
        .into_iter()
        .map(|item| item.trim().to_ascii_lowercase())
        .filter(|item| !item.is_empty())
        .take(64)
        .collect();
    let default_translate_fallback = default_translate_target_for_language(&settings.language);
    settings.selection_assistant.default_translate_to = normalize_translate_language(
        &settings.selection_assistant.default_translate_to,
        default_translate_fallback,
    );

    settings.llm.base_url = settings.llm.base_url.trim().to_string();
    if settings.llm.base_url.is_empty() {
        settings.llm.base_url = "https://api.openai.com/v1/chat/completions".to_string();
    }
    settings.llm.enabled = true;
    settings.llm.model = settings.llm.model.trim().to_string();
    if settings.llm.model.is_empty() {
        settings.llm.model = "gpt-4o-mini".to_string();
    }
    settings.llm.temperature = settings.llm.temperature.clamp(0.0, 2.0);
    settings.llm.max_tokens = settings.llm.max_tokens.clamp(128, 8192);
    settings.llm.timeout_ms = settings.llm.timeout_ms.clamp(5_000, 120_000);

    settings.tts.voice_zh_cn =
        normalize_tts_voice(&settings.tts.voice_zh_cn, DEFAULT_TTS_VOICE_ZH_CN);
    settings.tts.voice_en_us =
        normalize_tts_voice(&settings.tts.voice_en_us, DEFAULT_TTS_VOICE_EN_US);
    settings.tts.rate_percent = settings
        .tts
        .rate_percent
        .clamp(MIN_TTS_RATE_PERCENT, MAX_TTS_RATE_PERCENT);

    settings.ocr.custom_agent_id = settings.ocr.custom_agent_id.trim().to_string();
    settings.ocr.vision.api_key = settings.ocr.vision.api_key.trim().to_string();
    settings.ocr.vision.base_url = settings.ocr.vision.base_url.trim().to_string();
    if settings.ocr.vision.base_url.is_empty() {
        settings.ocr.vision.base_url = "https://api.openai.com/v1/chat/completions".to_string();
    }
    settings.ocr.vision.model = settings.ocr.vision.model.trim().to_string();
    if settings.ocr.vision.model.is_empty() {
        settings.ocr.vision.model = "gpt-4o-mini".to_string();
    }
    settings.ocr.vision.temperature = settings.ocr.vision.temperature.clamp(0.0, 2.0);
    settings.ocr.vision.max_tokens = settings.ocr.vision.max_tokens.clamp(256, 8192);
    settings.ocr.vision.timeout_ms = settings.ocr.vision.timeout_ms.clamp(5_000, 120_000);
    settings.ocr.vision.enabled = !settings.ocr.vision.api_key.is_empty();
    if settings.ocr.default_action == OcrDefaultAction::Custom
        && settings.ocr.custom_agent_id.is_empty()
    {
        settings.ocr.default_action = OcrDefaultAction::Translate;
    }

    let custom_agents = std::mem::take(&mut settings.agents.custom);
    settings.agents.custom = custom_agents
        .into_iter()
        .enumerate()
        .map(|(index, mut agent)| {
            if agent.id.trim().is_empty() {
                agent.id = format!("agent-{}-{}", index, now_epoch_millis());
            } else {
                agent.id = agent.id.trim().to_string();
            }
            if agent.name.trim().is_empty() {
                agent.name = format!("Agent {}", index + 1);
            } else {
                let trimmed = trim_name_by_units(&agent.name, 8);
                agent.name = if trimmed.is_empty() {
                    format!("Agent {}", index + 1)
                } else {
                    trimmed
                };
            }
            agent.icon = {
                let icon = agent.icon.trim();
                if icon.is_empty() {
                    "Sparkles".to_string()
                } else {
                    icon.to_string()
                }
            };
            agent.prompt = agent.prompt.trim().to_string();
            agent.enabled = true;
            agent.order = index as u32;
            agent
        })
        .take(30)
        .collect();
    let raw_bar_order = std::mem::take(&mut settings.agents.bar_order);
    settings.agents.bar_order =
        normalize_selection_bar_order(raw_bar_order, &settings.agents.custom);
}

fn apply_settings_patch(settings: &mut AppSettings, patch: SettingsPatch) {
    if let Some(theme_preset) = patch.theme_preset {
        settings.theme_preset = theme_preset;
    }

    if let Some(language) = patch.language {
        settings.language = language;
    }

    if let Some(window_patch) = patch.window {
        if let Some(enabled) = window_patch.auto_hide_on_blur {
            settings.window.auto_hide_on_blur = enabled;
        }
        if let Some(enabled) = window_patch.remember_position {
            settings.window.remember_position = enabled;
        }
        if let Some(enabled) = window_patch.remember_main_window_size {
            settings.window.remember_main_window_size = enabled;
        }
        if let Some(enabled) = window_patch.launch_on_system_startup {
            settings.window.launch_on_system_startup = enabled;
        }
        if let Some(enabled) = window_patch.silent_startup {
            settings.window.silent_startup = enabled;
        }
        if let Some(enabled) = window_patch.check_updates_on_startup {
            settings.window.check_updates_on_startup = enabled;
        }
    }

    if let Some(selection_patch) = patch.selection_assistant {
        if let Some(enabled) = selection_patch.enabled {
            settings.selection_assistant.enabled = enabled;
        }
        if let Some(mode) = selection_patch.mode {
            settings.selection_assistant.mode = mode;
        }
        if let Some(show) = selection_patch.show_icon_animation {
            settings.selection_assistant.show_icon_animation = show;
        }
        if let Some(compact) = selection_patch.compact_mode {
            settings.selection_assistant.compact_mode = compact;
        }
        if let Some(ms) = selection_patch.auto_hide_ms {
            settings.selection_assistant.auto_hide_ms = ms;
        }
        if let Some(template) = selection_patch.search_url_template {
            settings.selection_assistant.search_url_template = template;
        }
        if let Some(min_chars) = selection_patch.min_chars {
            settings.selection_assistant.min_chars = min_chars;
        }
        if let Some(max_chars) = selection_patch.max_chars {
            settings.selection_assistant.max_chars = max_chars;
        }
        if let Some(blocked_apps) = selection_patch.blocked_apps {
            settings.selection_assistant.blocked_apps = blocked_apps;
        }
        if let Some(default_translate_to) = selection_patch.default_translate_to {
            settings.selection_assistant.default_translate_to = default_translate_to;
        }
        if let Some(pinned) = selection_patch.result_window_always_on_top {
            settings.selection_assistant.result_window_always_on_top = pinned;
        }
        if let Some(remember_position) = selection_patch.remember_result_window_position {
            settings.selection_assistant.remember_result_window_position = remember_position;
        }
    }

    if let Some(llm_patch) = patch.llm {
        let _ = llm_patch.enabled;
        if let Some(base_url) = llm_patch.base_url {
            settings.llm.base_url = base_url;
        }
        if let Some(api_key) = llm_patch.api_key {
            settings.llm.api_key = api_key;
        }
        if let Some(model) = llm_patch.model {
            settings.llm.model = model;
        }
        if let Some(temperature) = llm_patch.temperature {
            settings.llm.temperature = temperature;
        }
        if let Some(max_tokens) = llm_patch.max_tokens {
            settings.llm.max_tokens = max_tokens;
        }
        if let Some(timeout_ms) = llm_patch.timeout_ms {
            settings.llm.timeout_ms = timeout_ms;
        }
    }

    if let Some(tts_patch) = patch.tts {
        if let Some(runtime_mode) = tts_patch.runtime_mode {
            settings.tts.runtime_mode = runtime_mode;
        }
        if let Some(voice_zh_cn) = tts_patch.voice_zh_cn {
            settings.tts.voice_zh_cn = voice_zh_cn;
        }
        if let Some(voice_en_us) = tts_patch.voice_en_us {
            settings.tts.voice_en_us = voice_en_us;
        }
        if let Some(rate_percent) = tts_patch.rate_percent {
            settings.tts.rate_percent = rate_percent;
        }
    }

    if let Some(agent_patch) = patch.agents {
        if let Some(custom) = agent_patch.custom {
            settings.agents.custom = custom;
        }
        if let Some(bar_order) = agent_patch.bar_order {
            settings.agents.bar_order = bar_order;
        }
    }

    if let Some(shortcuts_patch) = patch.shortcuts {
        if let Some(shortcut) = shortcuts_patch.toggle_main {
            settings.shortcuts.toggle_main = shortcut;
        }
        if let Some(shortcut) = shortcuts_patch.toggle_ocr {
            settings.shortcuts.toggle_ocr = shortcut;
        }
    }

    if let Some(ocr_patch) = patch.ocr {
        if let Some(enabled) = ocr_patch.enabled {
            settings.ocr.enabled = enabled;
        }
        if let Some(auto_run) = ocr_patch.auto_run_after_capture {
            settings.ocr.auto_run_after_capture = auto_run;
        }
        if let Some(action) = ocr_patch.default_action {
            settings.ocr.default_action = action;
        }
        if let Some(custom_agent_id) = ocr_patch.custom_agent_id {
            settings.ocr.custom_agent_id = custom_agent_id;
        }
        if let Some(always_on_top) = ocr_patch.result_window_always_on_top {
            settings.ocr.result_window_always_on_top = always_on_top;
        }
        if let Some(remember_position) = ocr_patch.remember_result_window_position {
            settings.ocr.remember_result_window_position = remember_position;
        }
        if let Some(vision_patch) = ocr_patch.vision {
            if let Some(enabled) = vision_patch.enabled {
                settings.ocr.vision.enabled = enabled;
            }
            if let Some(base_url) = vision_patch.base_url {
                settings.ocr.vision.base_url = base_url;
            }
            if let Some(api_key) = vision_patch.api_key {
                settings.ocr.vision.api_key = api_key;
            }
            if let Some(model) = vision_patch.model {
                settings.ocr.vision.model = model;
            }
            if let Some(temperature) = vision_patch.temperature {
                settings.ocr.vision.temperature = temperature;
            }
            if let Some(max_tokens) = vision_patch.max_tokens {
                settings.ocr.vision.max_tokens = max_tokens;
            }
            if let Some(timeout_ms) = vision_patch.timeout_ms {
                settings.ocr.vision.timeout_ms = timeout_ms;
            }
        }
    }

    if let Some(history_patch) = patch.history {
        if let Some(poll_ms) = history_patch.poll_ms {
            settings.history.poll_ms = poll_ms;
        }
        if let Some(max_items) = history_patch.max_items {
            settings.history.max_items = max_items;
        }
        if let Some(dedupe) = history_patch.dedupe {
            settings.history.dedupe = dedupe;
        }
        if let Some(enabled) = history_patch.capture_text {
            settings.history.capture_text = enabled;
        }
        if let Some(enabled) = history_patch.capture_link {
            settings.history.capture_link = enabled;
        }
        if let Some(enabled) = history_patch.capture_image {
            settings.history.capture_image = enabled;
        }
        if let Some(default_open_category) = history_patch.default_open_category {
            settings.history.default_open_category = default_open_category;
        }
        if let Some(default_category) = history_patch.default_category {
            settings.history.default_category = default_category;
        }
        if let Some(paste_behavior) = history_patch.paste_behavior {
            settings.history.paste_behavior = paste_behavior;
        }
        if let Some(collapse_top_bar) = history_patch.collapse_top_bar {
            settings.history.collapse_top_bar = collapse_top_bar;
        }
        if let Some(promote_after_paste) = history_patch.promote_after_paste {
            settings.history.promote_after_paste = promote_after_paste;
        }
        if let Some(open_at_top_on_show) = history_patch.open_at_top_on_show {
            settings.history.open_at_top_on_show = open_at_top_on_show;
        }
        if let Some(storage_path) = history_patch.storage_path {
            settings.history.storage_path = storage_path;
        }
    }

    if let Some(width) = patch.selection_result_window_width {
        settings.selection_result_window_width = Some(width);
    }
    if let Some(height) = patch.selection_result_window_height {
        settings.selection_result_window_height = Some(height);
    }
    if let Some(width) = patch.ocr_result_window_width {
        settings.ocr_result_window_width = Some(width);
    }
    if let Some(height) = patch.ocr_result_window_height {
        settings.ocr_result_window_height = Some(height);
    }

    normalize_settings(settings);
}
fn settings_file_path<R: Runtime>(app: &AppHandle<R>) -> Result<PathBuf, CommandError> {
    let mut dir = app
        .path()
        .app_config_dir()
        .map_err(|error| CommandError::Settings(error.to_string()))?;
    fs::create_dir_all(&dir).map_err(|error| CommandError::Settings(error.to_string()))?;
    dir.push(SETTINGS_FILENAME);
    Ok(dir)
}

fn settings_backup_path(path: &Path) -> PathBuf {
    let mut backup = path.to_path_buf();
    backup.set_file_name(SETTINGS_BACKUP_FILENAME);
    backup
}

fn history_backup_path(path: &Path) -> PathBuf {
    let mut backup = path.to_path_buf();
    let base_name = path
        .file_name()
        .and_then(|name| name.to_str())
        .unwrap_or(HISTORY_FILENAME);
    backup.set_file_name(format!("{base_name}.bak"));
    backup
}

fn normalize_enum_value_in_object(
    object: &mut serde_json::Map<String, serde_json::Value>,
    key: &str,
    allowed: &[&str],
    default: &str,
) -> bool {
    let normalized = object
        .get(key)
        .and_then(|value| value.as_str())
        .map(|value| value.trim().to_ascii_lowercase());

    let should_replace = match normalized {
        Some(value) => !allowed.iter().any(|candidate| *candidate == value),
        None => true,
    };

    if should_replace {
        object.insert(
            key.to_string(),
            serde_json::Value::String(default.to_string()),
        );
        return true;
    }

    false
}

fn parse_bool_like_json(value: &serde_json::Value) -> Option<bool> {
    if let Some(flag) = value.as_bool() {
        return Some(flag);
    }
    if let Some(number) = value.as_i64() {
        return Some(number != 0);
    }
    if let Some(text) = value.as_str() {
        return match text.trim().to_ascii_lowercase().as_str() {
            "1" | "true" | "yes" | "on" => Some(true),
            "0" | "false" | "no" | "off" => Some(false),
            _ => None,
        };
    }
    None
}

fn assign_window_bool_if_missing(
    window_obj: &mut serde_json::Map<String, serde_json::Value>,
    target_key: &str,
    flag: bool,
) -> bool {
    let has_valid_value = window_obj
        .get(target_key)
        .and_then(parse_bool_like_json)
        .is_some();
    if has_valid_value {
        return false;
    }
    window_obj.insert(target_key.to_string(), serde_json::Value::Bool(flag));
    true
}

fn migrate_legacy_theme_preset(root: &mut serde_json::Map<String, serde_json::Value>) -> bool {
    let incoming = root
        .get("themePreset")
        .and_then(|value| value.as_str())
        .map(|value| value.trim().to_ascii_lowercase())
        .or_else(|| {
            root.get("theme")
                .and_then(|value| value.as_str())
                .map(|value| value.trim().to_ascii_lowercase())
        });

    let Some(theme) = incoming else {
        return false;
    };

    let mapped = match theme.as_str() {
        "black" | "dark" => "deep-black",
        "md2-dark" | "midnight" => "blue",
        "graphite" => "gray",
        "daylight" | "sunrise" | "amber-mist" => "white",
        "blue" | "deep-black" | "gray" | "white" => theme.as_str(),
        _ => return false,
    };

    let current = root
        .get("themePreset")
        .and_then(|value| value.as_str())
        .map(|value| value.trim().to_ascii_lowercase());
    if current.as_deref() == Some(mapped) {
        return false;
    }

    root.insert(
        "themePreset".to_string(),
        serde_json::Value::String(mapped.to_string()),
    );
    true
}

fn migrate_legacy_window_settings(root: &mut serde_json::Map<String, serde_json::Value>) -> bool {
    let mut window_obj = root
        .remove("window")
        .and_then(|value| value.as_object().cloned())
        .unwrap_or_default();

    let mut changed = false;

    for (legacy_key, target_key) in [
        ("launchOnSystemStartup", "launchOnSystemStartup"),
        ("launchOnStartup", "launchOnSystemStartup"),
        ("launch_on_system_startup", "launchOnSystemStartup"),
        ("silentStartup", "silentStartup"),
        ("silentOnStartup", "silentStartup"),
        ("silent_startup", "silentStartup"),
        ("checkUpdatesOnStartup", "checkUpdatesOnStartup"),
        ("checkUpdatesOnLaunch", "checkUpdatesOnStartup"),
        ("check_updates_on_startup", "checkUpdatesOnStartup"),
        ("autoHideOnBlur", "autoHideOnBlur"),
        ("auto_hide_on_blur", "autoHideOnBlur"),
        ("rememberPosition", "rememberPosition"),
        ("remember_position", "rememberPosition"),
        ("rememberMainWindowSize", "rememberMainWindowSize"),
        ("remember_main_window_size", "rememberMainWindowSize"),
    ] {
        let root_flag = root.get(legacy_key).and_then(parse_bool_like_json);
        let nested_flag = window_obj.get(legacy_key).and_then(parse_bool_like_json);
        if let Some(flag) = root_flag {
            changed |= assign_window_bool_if_missing(&mut window_obj, target_key, flag);
        }
        if let Some(flag) = nested_flag {
            changed |= assign_window_bool_if_missing(&mut window_obj, target_key, flag);
        }
    }

    if !window_obj.is_empty() {
        root.insert("window".to_string(), serde_json::Value::Object(window_obj));
    }

    changed
}

fn normalize_settings_json_value(value: &mut serde_json::Value) -> bool {
    let Some(root) = value.as_object_mut() else {
        return false;
    };

    let mut changed = false;

    changed |= migrate_legacy_theme_preset(root);
    changed |= migrate_legacy_window_settings(root);

    changed |= normalize_enum_value_in_object(
        root,
        "themePreset",
        &[
            "blue",
            "deep-black",
            "gray",
            "white",
            "black",
            "md2-dark",
            "midnight",
            "graphite",
            "daylight",
            "sunrise",
            "amber-mist",
        ],
        "deep-black",
    );

    if let Some(selection) = root
        .get_mut("selectionAssistant")
        .and_then(|value| value.as_object_mut())
    {
        changed |= normalize_enum_value_in_object(
            selection,
            "mode",
            &["auto-detect", "copy-trigger"],
            "auto-detect",
        );
    }

    if let Some(history) = root
        .get_mut("history")
        .and_then(|value| value.as_object_mut())
    {
        if !history.contains_key("defaultOpenCategory") {
            if let Some(existing_default) = history
                .get("defaultCategory")
                .and_then(|value| value.as_str())
                .map(|value| value.trim().to_ascii_lowercase())
            {
                let migrated = match existing_default.as_str() {
                    "all" | "text" | "link" | "image" | "favorite" => {
                        Some(existing_default.as_str())
                    }
                    _ => None,
                };
                if let Some(migrated) = migrated {
                    history.insert(
                        "defaultOpenCategory".to_string(),
                        serde_json::Value::String(migrated.to_string()),
                    );
                    changed = true;
                }
            }
        }
        changed |= normalize_enum_value_in_object(
            history,
            "defaultOpenCategory",
            &["all", "text", "link", "image", "favorite", "last-used"],
            "all",
        );
        changed |= normalize_enum_value_in_object(
            history,
            "defaultCategory",
            &["all", "text", "link", "image", "favorite"],
            "all",
        );
        changed |= normalize_enum_value_in_object(
            history,
            "pasteBehavior",
            &["copy-only", "copy-and-hide"],
            "copy-and-hide",
        );
    }

    if let Some(ocr) = root.get_mut("ocr").and_then(|value| value.as_object_mut()) {
        changed |= normalize_enum_value_in_object(
            ocr,
            "defaultAction",
            &["translate", "summarize", "polish", "explain", "custom"],
            "translate",
        );
    }

    if let Some(tts) = root.get_mut("tts").and_then(|value| value.as_object_mut()) {
        changed |= normalize_enum_value_in_object(
            tts,
            "runtimeMode",
            &["dual-fallback", "edge-cli-only", "python-module-only"],
            "dual-fallback",
        );
    }

    changed
}

fn merge_json_value_by_shape(target: &mut serde_json::Value, incoming: &serde_json::Value) {
    if let (Some(target_map), Some(incoming_map)) = (target.as_object_mut(), incoming.as_object()) {
        for (key, incoming_value) in incoming_map {
            if let Some(target_value) = target_map.get_mut(key) {
                merge_json_value_by_shape(target_value, incoming_value);
            } else {
                target_map.insert(key.clone(), incoming_value.clone());
            }
        }
        return;
    }

    let should_replace = target.is_null()
        || (target.is_array() && incoming.is_array())
        || (target.is_string() && incoming.is_string())
        || (target.is_boolean() && incoming.is_boolean())
        || (target.is_number() && incoming.is_number());
    if should_replace {
        *target = incoming.clone();
    }
}

fn parse_settings_from_text(text: &str) -> Result<(AppSettings, bool), CommandError> {
    let mut incoming = serde_json::from_str::<serde_json::Value>(text)
        .map_err(|error| CommandError::Serialization(error.to_string()))?;
    let direct_parse_ok = serde_json::from_str::<AppSettings>(text).is_ok();
    let repaired_json = normalize_settings_json_value(&mut incoming);

    let mut merged = serde_json::to_value(AppSettings::default())
        .map_err(|error| CommandError::Serialization(error.to_string()))?;
    merge_json_value_by_shape(&mut merged, &incoming);

    let mut settings = serde_json::from_value::<AppSettings>(merged)
        .map_err(|error| CommandError::Serialization(error.to_string()))?;
    normalize_settings(&mut settings);
    Ok((settings, repaired_json || !direct_parse_ok))
}

fn file_modified_epoch_ms(path: &Path) -> u64 {
    fs::metadata(path)
        .and_then(|metadata| metadata.modified())
        .ok()
        .and_then(|time| time.duration_since(UNIX_EPOCH).ok())
        .map(|duration| duration.as_millis() as u64)
        .unwrap_or(0)
}

fn is_likely_snapparse_dir(path: &Path) -> bool {
    path.file_name()
        .and_then(|name| name.to_str())
        .map(|name| {
            let lowered = name.to_ascii_lowercase();
            lowered.contains("snapparse") || lowered.contains("clipboard")
        })
        .unwrap_or(false)
}

fn discover_legacy_storage_dirs(settings_path: &Path) -> Vec<PathBuf> {
    let Some(current_dir) = settings_path.parent() else {
        return Vec::new();
    };
    let Some(parent_dir) = current_dir.parent() else {
        return Vec::new();
    };

    let mut dirs = Vec::<PathBuf>::new();
    if let Ok(entries) = fs::read_dir(parent_dir) {
        for entry in entries.flatten() {
            let candidate = entry.path();
            if !candidate.is_dir() || candidate == current_dir {
                continue;
            }
            if !is_likely_snapparse_dir(&candidate) {
                continue;
            }
            dirs.push(candidate);
        }
    }
    dirs.sort_by_key(|path| std::cmp::Reverse(file_modified_epoch_ms(path)));
    dirs
}

fn dedupe_paths_keep_order(input: Vec<PathBuf>) -> Vec<PathBuf> {
    let mut seen = HashSet::<PathBuf>::new();
    let mut output = Vec::<PathBuf>::new();
    for item in input {
        if seen.insert(item.clone()) {
            output.push(item);
        }
    }
    output
}

fn parse_history_kind(value: &serde_json::Value) -> Option<ClipboardKind> {
    let raw = value.as_str()?.trim().to_ascii_lowercase();
    match raw.as_str() {
        "text" => Some(ClipboardKind::Text),
        "link" => Some(ClipboardKind::Link),
        "image" => Some(ClipboardKind::Image),
        _ => None,
    }
}

fn parse_history_timestamp(value: &serde_json::Value) -> Option<DateTime<Utc>> {
    if let Some(text) = value.as_str() {
        if let Ok(parsed) = DateTime::parse_from_rfc3339(text.trim()) {
            return Some(parsed.with_timezone(&Utc));
        }
        return None;
    }

    let numeric = value.as_i64()?;
    if numeric > 4_000_000_000 {
        DateTime::<Utc>::from_timestamp_millis(numeric)
    } else {
        DateTime::<Utc>::from_timestamp(numeric, 0)
    }
}

fn parse_history_entry_compat(raw: &serde_json::Value) -> Option<ClipboardEntry> {
    let object = raw.as_object()?;
    let content = object
        .get("content")
        .and_then(|value| value.as_str())
        .or_else(|| object.get("text").and_then(|value| value.as_str()))
        .unwrap_or_default()
        .to_string();
    let image_data_url = object
        .get("imageDataUrl")
        .and_then(|value| value.as_str())
        .or_else(|| object.get("image").and_then(|value| value.as_str()))
        .map(str::to_string);

    let kind = object
        .get("kind")
        .and_then(parse_history_kind)
        .or_else(|| {
            if image_data_url.is_some() {
                Some(ClipboardKind::Image)
            } else if content.contains("://") {
                Some(ClipboardKind::Link)
            } else {
                Some(ClipboardKind::Text)
            }
        })?;

    let copied_at = object
        .get("copiedAt")
        .and_then(parse_history_timestamp)
        .or_else(|| object.get("copied_at").and_then(parse_history_timestamp))
        .or_else(|| object.get("createdAt").and_then(parse_history_timestamp))
        .or_else(|| object.get("timestamp").and_then(parse_history_timestamp))
        .unwrap_or_else(Utc::now);

    let pinned = object
        .get("pinned")
        .and_then(parse_bool_like_json)
        .unwrap_or(false);
    let id = object
        .get("id")
        .and_then(|value| value.as_str())
        .map(str::trim)
        .filter(|value| !value.is_empty())
        .map(str::to_string)
        .unwrap_or_else(now_id);

    Some(ClipboardEntry {
        id,
        kind,
        content,
        image_data_url,
        copied_at,
        pinned,
    })
}

fn parse_history_entries(
    text: &str,
    max_items: usize,
) -> Result<VecDeque<ClipboardEntry>, CommandError> {
    let mut items = match serde_json::from_str::<Vec<ClipboardEntry>>(text) {
        Ok(parsed) => parsed,
        Err(_) => {
            let raw_value = serde_json::from_str::<serde_json::Value>(text)
                .map_err(|error| CommandError::Serialization(error.to_string()))?;
            let raw_items = raw_value.as_array().ok_or_else(|| {
                CommandError::Serialization("History snapshot must be a JSON array".to_string())
            })?;
            let mut repaired = Vec::<ClipboardEntry>::with_capacity(raw_items.len());
            for raw_item in raw_items {
                if let Ok(entry) = serde_json::from_value::<ClipboardEntry>(raw_item.clone()) {
                    repaired.push(entry);
                    continue;
                }
                if let Some(entry) = parse_history_entry_compat(raw_item) {
                    repaired.push(entry);
                }
            }
            repaired
        }
    };
    items.retain(|item| item.kind == ClipboardKind::Image || !item.content.trim().is_empty());

    let mut history = VecDeque::from(items);
    trim_history(&mut history, max_items);
    normalize_history_order(&mut history);
    Ok(history)
}

fn read_text_with_retry(path: &Path) -> Result<String, std::io::Error> {
    const RETRY_COUNT: usize = 20;
    const RETRY_DELAY_MS: u64 = 120;
    let mut last_error: Option<std::io::Error> = None;

    for attempt in 0..RETRY_COUNT {
        match fs::read_to_string(path) {
            Ok(value) => return Ok(value),
            Err(error)
                if matches!(
                    error.kind(),
                    ErrorKind::PermissionDenied | ErrorKind::WouldBlock | ErrorKind::Interrupted
                ) && attempt + 1 < RETRY_COUNT =>
            {
                last_error = Some(error);
                std::thread::sleep(Duration::from_millis(RETRY_DELAY_MS));
            }
            Err(error) => return Err(error),
        }
    }

    Err(last_error
        .unwrap_or_else(|| std::io::Error::other("history file read failed after retries")))
}

fn resolve_history_path_from_base_dir(storage_path: &str, base_dir: &Path) -> PathBuf {
    let trimmed = storage_path.trim();
    if trimmed.is_empty() {
        return base_dir.join(HISTORY_FILENAME);
    }

    let mut path = PathBuf::from(trimmed);
    if path.is_relative() {
        path = base_dir.join(path);
    }

    if !trimmed.to_ascii_lowercase().ends_with(".json") {
        path.push(HISTORY_FILENAME);
    }
    path
}

fn app_config_history_file_path<R: Runtime>(app: &AppHandle<R>) -> Result<PathBuf, CommandError> {
    let mut config_dir = app
        .path()
        .app_config_dir()
        .map_err(|error| CommandError::Settings(error.to_string()))?;
    fs::create_dir_all(&config_dir).map_err(|error| CommandError::Settings(error.to_string()))?;
    config_dir.push(HISTORY_FILENAME);
    Ok(config_dir)
}

fn resolve_history_file_path<R: Runtime>(
    app: &AppHandle<R>,
    settings: &AppSettings,
) -> Result<PathBuf, CommandError> {
    let config_dir = app_config_history_file_path(app)?
        .parent()
        .map(Path::to_path_buf)
        .ok_or_else(|| CommandError::Settings("Invalid app config directory".to_string()))?;
    let path = resolve_history_path_from_base_dir(&settings.history.storage_path, &config_dir);

    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|error| CommandError::Settings(error.to_string()))?;
    }

    Ok(path)
}

fn persist_settings(path: &Path, settings: &AppSettings) -> Result<(), CommandError> {
    let payload = serde_json::to_string_pretty(settings)
        .map_err(|error| CommandError::Serialization(error.to_string()))?;

    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|error| CommandError::Settings(error.to_string()))?;
    }

    let backup_path = settings_backup_path(path);
    if path.exists() {
        let _ = fs::copy(path, &backup_path);
    }

    let temp_path = path.with_extension("tmp");
    fs::write(&temp_path, &payload).map_err(|error| CommandError::Settings(error.to_string()))?;

    if path.exists() {
        let _ = fs::remove_file(path);
    }

    match fs::rename(&temp_path, path) {
        Ok(_) => Ok(()),
        Err(_) => {
            fs::write(path, payload).map_err(|error| CommandError::Settings(error.to_string()))?;
            let _ = fs::remove_file(&temp_path);
            Ok(())
        }
    }
}

fn persist_history_snapshot<R: Runtime>(
    app: &AppHandle<R>,
    settings: &AppSettings,
    entries: &[ClipboardEntry],
) -> Result<(), CommandError> {
    let payload = serde_json::to_string_pretty(entries)
        .map_err(|error| CommandError::Serialization(error.to_string()))?;
    let write_snapshot = |path: &Path, payload: &str| -> Result<(), CommandError> {
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)
                .map_err(|error| CommandError::Settings(error.to_string()))?;
        }
        let backup_path = history_backup_path(path);
        if path.exists() {
            let _ = fs::copy(path, backup_path);
        }

        let temp_path = path.with_extension("tmp");
        fs::write(&temp_path, payload)
            .map_err(|error| CommandError::Settings(error.to_string()))?;

        if path.exists() {
            let _ = fs::remove_file(path);
        }

        match fs::rename(&temp_path, path) {
            Ok(_) => Ok(()),
            Err(_) => {
                fs::write(path, payload)
                    .map_err(|error| CommandError::Settings(error.to_string()))?;
                let _ = fs::remove_file(&temp_path);
                Ok(())
            }
        }
    };

    let primary_path = resolve_history_file_path(app, settings)?;
    let app_config_path = app_config_history_file_path(app)?;
    let primary_result = write_snapshot(&primary_path, &payload);
    let mirror_result = if app_config_path != primary_path {
        write_snapshot(&app_config_path, &payload)
    } else {
        Ok(())
    };

    match (primary_result, mirror_result) {
        (Ok(_), _) => Ok(()),
        (Err(primary_error), Ok(_)) => {
            eprintln!(
                "[History] primary snapshot write failed, mirrored to app config: {}",
                primary_error
            );
            Ok(())
        }
        (Err(primary_error), Err(_mirror_error)) => Err(primary_error),
    }
}

fn load_history_snapshot<R: Runtime>(
    app: &AppHandle<R>,
    settings: &AppSettings,
    canonical_settings_path: &Path,
    settings_source_path: &Path,
) -> Result<HistoryLoadResult, CommandError> {
    let canonical_history_path = resolve_history_file_path(app, settings)?;
    let app_config_history_path = app_config_history_file_path(app)?;
    let mut empty_history_candidate: Option<HistoryLoadResult> = None;

    if let Ok(text) = read_text_with_retry(&canonical_history_path) {
        if let Ok(history) = parse_history_entries(&text, settings.history.max_items) {
            if !history.is_empty() {
                return Ok(HistoryLoadResult {
                    history,
                    source_path: Some(canonical_history_path.clone()),
                    should_persist: false,
                });
            }
            empty_history_candidate = Some(HistoryLoadResult {
                history,
                source_path: Some(canonical_history_path.clone()),
                should_persist: false,
            });
        }
        eprintln!(
            "[History] primary snapshot parse failed {}",
            canonical_history_path.display()
        );
    }

    let canonical_backup_path = history_backup_path(&canonical_history_path);
    if let Ok(text) = read_text_with_retry(&canonical_backup_path) {
        if let Ok(history) = parse_history_entries(&text, settings.history.max_items) {
            if !history.is_empty() {
                return Ok(HistoryLoadResult {
                    history,
                    source_path: Some(canonical_backup_path.clone()),
                    should_persist: true,
                });
            }
            if empty_history_candidate.is_none() {
                empty_history_candidate = Some(HistoryLoadResult {
                    history,
                    source_path: Some(canonical_backup_path.clone()),
                    should_persist: true,
                });
            }
        }
        eprintln!(
            "[History] backup snapshot parse failed {}",
            canonical_backup_path.display()
        );
    }

    let mut fallback_candidates = Vec::<PathBuf>::new();
    if app_config_history_path != canonical_history_path {
        fallback_candidates.push(app_config_history_path.clone());
        fallback_candidates.push(history_backup_path(&app_config_history_path));
    }
    if let (Some(source_dir), Some(canonical_dir)) = (
        settings_source_path.parent(),
        canonical_settings_path.parent(),
    ) {
        if source_dir != canonical_dir {
            let source_history_path =
                resolve_history_path_from_base_dir(&settings.history.storage_path, source_dir);
            fallback_candidates.push(source_history_path.clone());
            fallback_candidates.push(history_backup_path(&source_history_path));
        }
    }

    for legacy_dir in discover_legacy_storage_dirs(canonical_settings_path) {
        let legacy_history_path =
            resolve_history_path_from_base_dir(&settings.history.storage_path, &legacy_dir);
        fallback_candidates.push(legacy_history_path.clone());
        fallback_candidates.push(history_backup_path(&legacy_history_path));
    }

    let mut fallback_candidates = dedupe_paths_keep_order(fallback_candidates);
    fallback_candidates.sort_by_key(|path| std::cmp::Reverse(file_modified_epoch_ms(path)));

    for candidate in fallback_candidates {
        if let Ok(text) = read_text_with_retry(&candidate) {
            if let Ok(history) = parse_history_entries(&text, settings.history.max_items) {
                if !history.is_empty() {
                    return Ok(HistoryLoadResult {
                        history,
                        source_path: Some(candidate),
                        should_persist: true,
                    });
                }
                if empty_history_candidate.is_none() {
                    empty_history_candidate = Some(HistoryLoadResult {
                        history,
                        source_path: Some(candidate),
                        should_persist: true,
                    });
                }
            }
        }
    }

    if let Some(candidate) = empty_history_candidate {
        return Ok(candidate);
    }

    Ok(HistoryLoadResult {
        history: VecDeque::new(),
        source_path: None,
        should_persist: false,
    })
}

fn persist_settings_state(settings_state: &AppSettingsState) -> Result<(), CommandError> {
    let snapshot = settings_state
        .data
        .lock()
        .map_err(|_| CommandError::Lock)?
        .clone();
    persist_settings(&settings_state.file_path, &snapshot)
}

fn load_settings(path: &Path) -> SettingsLoadResult {
    let canonical_path = path.to_path_buf();
    let canonical_backup = settings_backup_path(path);
    let mut candidate_paths = vec![canonical_path.clone(), canonical_backup.clone()];
    for legacy_dir in discover_legacy_storage_dirs(path) {
        candidate_paths.push(legacy_dir.join(SETTINGS_FILENAME));
        candidate_paths.push(legacy_dir.join(SETTINGS_BACKUP_FILENAME));
    }
    candidate_paths = dedupe_paths_keep_order(candidate_paths);

    // Try canonical file and backup first to avoid restoring stale legacy snapshots.
    for candidate in [&canonical_path, &canonical_backup] {
        if let Ok(text) = read_text_with_retry(candidate) {
            match parse_settings_from_text(&text) {
                Ok((settings, repaired)) => {
                    return SettingsLoadResult {
                        settings,
                        source_path: candidate.clone(),
                        should_persist: repaired || candidate.as_path() != canonical_path.as_path(),
                    };
                }
                Err(error) => {
                    eprintln!("[Settings] parse failed {}: {}", candidate.display(), error);
                }
            }
        }
    }

    let mut legacy_candidates = candidate_paths
        .into_iter()
        .filter(|candidate| candidate != &canonical_path && candidate != &canonical_backup)
        .collect::<Vec<_>>();
    legacy_candidates.sort_by_key(|candidate| std::cmp::Reverse(file_modified_epoch_ms(candidate)));

    for candidate in legacy_candidates {
        if let Ok(text) = read_text_with_retry(&candidate) {
            match parse_settings_from_text(&text) {
                Ok((settings, _repaired)) => {
                    return SettingsLoadResult {
                        settings,
                        source_path: candidate,
                        should_persist: true,
                    };
                }
                Err(error) => {
                    eprintln!("[Settings] parse failed {}: {}", candidate.display(), error);
                }
            }
        }
    }

    SettingsLoadResult {
        settings: AppSettings::default(),
        source_path: canonical_path,
        should_persist: true,
    }
}

fn emit_settings_updated<R: Runtime>(app: &AppHandle<R>, settings: &AppSettings) {
    let _ = app.emit(SETTINGS_UPDATED_EVENT, settings.clone());
}

#[cfg(target_os = "windows")]
fn capture_last_foreground_window(flags: &RuntimeFlags) {
    let hwnd = unsafe { GetForegroundWindow() };
    if !hwnd.is_null() {
        flags
            .last_foreground_hwnd
            .store(hwnd as isize, Ordering::Relaxed);
    }
}

#[cfg(not(target_os = "windows"))]
fn capture_last_foreground_window(_flags: &RuntimeFlags) {}

#[cfg(target_os = "windows")]
fn restore_foreground_window(hwnd_raw: isize) -> bool {
    if hwnd_raw == 0 {
        return false;
    }

    let hwnd = hwnd_raw as HWND;
    if hwnd.is_null() {
        return false;
    }

    if unsafe { IsWindow(hwnd) } == 0 {
        return false;
    }

    let foreground = unsafe { GetForegroundWindow() };
    if foreground == hwnd {
        return true;
    }

    unsafe {
        if IsIconic(hwnd) != 0 {
            ShowWindow(hwnd, SW_RESTORE);
        }
        SetForegroundWindow(hwnd) != 0
    }
}

#[cfg(not(target_os = "windows"))]
fn restore_foreground_window(_hwnd_raw: isize) -> bool {
    false
}

#[cfg(target_os = "windows")]
fn keyboard_input(vk: u16, key_up: bool) -> INPUT {
    INPUT {
        r#type: INPUT_KEYBOARD,
        Anonymous: INPUT_0 {
            ki: KEYBDINPUT {
                wVk: vk,
                wScan: 0,
                dwFlags: if key_up { KEYEVENTF_KEYUP } else { 0 },
                time: 0,
                dwExtraInfo: 0,
            },
        },
    }
}

#[cfg(target_os = "windows")]
fn send_system_paste_shortcut(hwnd_raw: isize) -> Result<(), CommandError> {
    const VK_VIRTUAL_V: u16 = 0x56;

    let _ = restore_foreground_window(hwnd_raw);
    std::thread::sleep(Duration::from_millis(18));

    let mut inputs = [
        keyboard_input(VK_CONTROL, false),
        keyboard_input(VK_VIRTUAL_V, false),
        keyboard_input(VK_VIRTUAL_V, true),
        keyboard_input(VK_CONTROL, true),
    ];

    let sent = unsafe {
        SendInput(
            inputs.len() as u32,
            inputs.as_mut_ptr(),
            std::mem::size_of::<INPUT>() as i32,
        )
    };

    if sent == inputs.len() as u32 {
        Ok(())
    } else {
        Err(CommandError::Settings(
            "Failed to dispatch Ctrl+V shortcut".to_string(),
        ))
    }
}

#[cfg(not(target_os = "windows"))]
fn send_system_paste_shortcut(_hwnd_raw: isize) -> Result<(), CommandError> {
    Ok(())
}

#[cfg(target_os = "windows")]
fn send_system_copy_shortcut(hwnd_raw: isize) -> Result<(), CommandError> {
    const VK_VIRTUAL_C: u16 = 0x43;
    const VK_VIRTUAL_INSERT: u16 = VK_INSERT;
    const VK_VIRTUAL_SHIFT: u16 = VK_SHIFT;

    if hwnd_raw == 0 {
        return Err(CommandError::Settings(
            "No foreground window available for copy shortcut".to_string(),
        ));
    }

    let _ = restore_foreground_window(hwnd_raw);
    std::thread::sleep(Duration::from_millis(16));

    let send = |inputs: &mut [INPUT]| -> bool {
        let sent = unsafe {
            SendInput(
                inputs.len() as u32,
                inputs.as_mut_ptr(),
                std::mem::size_of::<INPUT>() as i32,
            )
        };
        sent == inputs.len() as u32
    };

    if is_windows_terminal_window(hwnd_raw) {
        let mut primary = [
            keyboard_input(VK_CONTROL, false),
            keyboard_input(VK_VIRTUAL_SHIFT, false),
            keyboard_input(VK_VIRTUAL_C, false),
            keyboard_input(VK_VIRTUAL_C, true),
            keyboard_input(VK_VIRTUAL_SHIFT, true),
            keyboard_input(VK_CONTROL, true),
        ];
        let primary_sent = send(&mut primary);
        std::thread::sleep(Duration::from_millis(10));
        let mut fallback = [
            keyboard_input(VK_CONTROL, false),
            keyboard_input(VK_VIRTUAL_INSERT, false),
            keyboard_input(VK_VIRTUAL_INSERT, true),
            keyboard_input(VK_CONTROL, true),
        ];
        let fallback_sent = send(&mut fallback);
        if primary_sent || fallback_sent {
            return Ok(());
        }
        return Err(CommandError::Settings(
            "Failed to dispatch terminal copy shortcut".to_string(),
        ));
    }

    if is_console_like_window(hwnd_raw) {
        let mut fallback = [
            keyboard_input(VK_CONTROL, false),
            keyboard_input(VK_VIRTUAL_INSERT, false),
            keyboard_input(VK_VIRTUAL_INSERT, true),
            keyboard_input(VK_CONTROL, true),
        ];
        if send(&mut fallback) {
            return Ok(());
        }
        return Err(CommandError::Settings(
            "Failed to dispatch console copy shortcut".to_string(),
        ));
    }

    let mut inputs = [
        keyboard_input(VK_CONTROL, false),
        keyboard_input(VK_VIRTUAL_C, false),
        keyboard_input(VK_VIRTUAL_C, true),
        keyboard_input(VK_CONTROL, true),
    ];
    if send(&mut inputs) {
        Ok(())
    } else {
        Err(CommandError::Settings(
            "Failed to dispatch Ctrl+C shortcut".to_string(),
        ))
    }
}

#[cfg(not(target_os = "windows"))]
fn send_system_copy_shortcut(_hwnd_raw: isize) -> Result<(), CommandError> {
    Ok(())
}

#[cfg(target_os = "windows")]
fn is_terminal_host_process(hwnd_raw: isize) -> bool {
    let (process_name, process_path) = window_process_identity(hwnd_raw);
    let Some(name) = process_name else {
        return false;
    };

    const TERMINAL_HOST_PROCESSES: &[&str] = &[
        "windowsterminal.exe",
        "wt.exe",
        "powershell.exe",
        "pwsh.exe",
        "cmd.exe",
        "conhost.exe",
        "wezterm.exe",
        "wezterm-gui.exe",
        "mintty.exe",
        "alacritty.exe",
        "tabby.exe",
        "hyper.exe",
        "conemu.exe",
        "conemu64.exe",
        "cmder.exe",
        "code.exe",
        "code-insiders.exe",
        "cursor.exe",
        "vscodium.exe",
        "windsurf.exe",
    ];

    if TERMINAL_HOST_PROCESSES
        .iter()
        .any(|candidate| name == *candidate)
    {
        return true;
    }

    process_path
        .as_deref()
        .is_some_and(|path| path.contains("\\windowsapps\\wt.exe"))
}

#[cfg(not(target_os = "windows"))]
fn is_terminal_host_process(_hwnd_raw: isize) -> bool {
    false
}

#[cfg(target_os = "windows")]
fn is_console_like_window(hwnd_raw: isize) -> bool {
    let class_like = window_class_name(hwnd_raw)
        .map(|class_name| {
            class_name.contains("CONSOLEWINDOWCLASS")
                || class_name.contains("CASCADIA_HOSTING_WINDOW_CLASS")
                || class_name.contains("PSEUDOCONSOLEWINDOW")
                || class_name.contains("VIRTUALCONSOLECLASS")
                || class_name.contains("MINTTY")
                || class_name.contains("WEZTERM")
                || class_name.contains("ALACRITTY")
        })
        .unwrap_or(false);

    class_like || is_terminal_host_process(hwnd_raw)
}

#[cfg(target_os = "windows")]
fn is_windows_terminal_window(hwnd_raw: isize) -> bool {
    let class_like = window_class_name(hwnd_raw)
        .map(|class_name| {
            class_name.contains("CASCADIA_HOSTING_WINDOW_CLASS")
                || class_name.contains("PSEUDOCONSOLEWINDOW")
        })
        .unwrap_or(false);

    if class_like {
        return true;
    }

    let (process_name, _) = window_process_identity(hwnd_raw);
    process_name
        .as_deref()
        .is_some_and(|name| name == "windowsterminal.exe" || name == "wt.exe")
}

#[cfg(not(target_os = "windows"))]
fn is_console_like_window(_hwnd_raw: isize) -> bool {
    false
}

#[cfg(not(target_os = "windows"))]
fn is_windows_terminal_window(_hwnd_raw: isize) -> bool {
    false
}

#[cfg(target_os = "windows")]
fn send_console_copy_shortcut(hwnd_raw: isize) -> Result<(), CommandError> {
    const VK_VIRTUAL_INSERT: u16 = VK_INSERT;

    if hwnd_raw == 0 {
        return Err(CommandError::Settings(
            "No foreground window available for console copy shortcut".to_string(),
        ));
    }

    let _ = restore_foreground_window(hwnd_raw);
    std::thread::sleep(Duration::from_millis(12));

    let send = |inputs: &mut [INPUT]| -> bool {
        let sent = unsafe {
            SendInput(
                inputs.len() as u32,
                inputs.as_mut_ptr(),
                std::mem::size_of::<INPUT>() as i32,
            )
        };
        sent == inputs.len() as u32
    };

    let mut copy_insert = [
        keyboard_input(VK_CONTROL, false),
        keyboard_input(VK_VIRTUAL_INSERT, false),
        keyboard_input(VK_VIRTUAL_INSERT, true),
        keyboard_input(VK_CONTROL, true),
    ];
    if send(&mut copy_insert) {
        return Ok(());
    }

    Err(CommandError::Settings(
        "Failed to dispatch safe console copy shortcut".to_string(),
    ))
}

#[cfg(not(target_os = "windows"))]
fn send_console_copy_shortcut(_hwnd_raw: isize) -> Result<(), CommandError> {
    Ok(())
}

#[cfg(target_os = "windows")]
fn window_class_name(hwnd_raw: isize) -> Option<String> {
    let hwnd = hwnd_raw as HWND;
    if hwnd_raw == 0 || hwnd.is_null() || unsafe { IsWindow(hwnd) } == 0 {
        return None;
    }

    let mut buffer = [0u16; 128];
    let class_len = unsafe { GetClassNameW(hwnd, buffer.as_mut_ptr(), buffer.len() as i32) };
    if class_len <= 0 {
        return None;
    }

    Some(String::from_utf16_lossy(&buffer[..class_len as usize]).to_ascii_uppercase())
}

#[cfg(target_os = "windows")]
fn window_process_identity(hwnd_raw: isize) -> (Option<String>, Option<String>) {
    let hwnd = hwnd_raw as HWND;
    if hwnd_raw == 0 || hwnd.is_null() || unsafe { IsWindow(hwnd) } == 0 {
        return (None, None);
    }

    let mut process_id = 0u32;
    unsafe {
        GetWindowThreadProcessId(hwnd, &mut process_id as *mut u32);
    }
    if process_id == 0 {
        return (None, None);
    }

    let handle = unsafe { OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, process_id) };
    if handle.is_null() {
        return (None, None);
    }

    let mut buffer = vec![0u16; 2048];
    let mut length = buffer.len() as u32;
    let success =
        unsafe { QueryFullProcessImageNameW(handle, 0, buffer.as_mut_ptr(), &mut length) } != 0;
    unsafe {
        CloseHandle(handle);
    }

    if !success || length == 0 {
        return (None, None);
    }

    let full_path = String::from_utf16_lossy(&buffer[..length as usize]).to_ascii_lowercase();
    let executable = Path::new(&full_path)
        .file_name()
        .map(|value| value.to_string_lossy().to_ascii_lowercase());
    (executable, Some(full_path))
}

#[cfg(target_os = "windows")]
fn is_window_blocked_by_apps(hwnd_raw: isize, blocked_apps: &[String]) -> bool {
    if blocked_apps.is_empty() || hwnd_raw == 0 {
        return false;
    }

    let class_name = window_class_name(hwnd_raw).map(|value| value.to_ascii_lowercase());
    let (process_name, process_path) = window_process_identity(hwnd_raw);

    blocked_apps.iter().any(|item| {
        let pattern = item.trim().to_ascii_lowercase();
        if pattern.is_empty() {
            return false;
        }
        class_name
            .as_deref()
            .is_some_and(|value| value.contains(&pattern))
            || process_name
                .as_deref()
                .is_some_and(|value| value.contains(&pattern))
            || process_path
                .as_deref()
                .is_some_and(|value| value.contains(&pattern))
    })
}

#[cfg(not(target_os = "windows"))]
fn is_window_blocked_by_apps(_hwnd_raw: isize, _blocked_apps: &[String]) -> bool {
    false
}

#[cfg(target_os = "windows")]
fn is_left_mouse_pressed() -> bool {
    (unsafe { GetAsyncKeyState(VK_LBUTTON as i32) } as u16 & 0x8000) != 0
}

#[cfg(not(target_os = "windows"))]
fn is_left_mouse_pressed() -> bool {
    false
}

#[cfg(target_os = "windows")]
fn is_escape_pressed() -> bool {
    (unsafe { GetAsyncKeyState(VK_ESCAPE as i32) } as u16 & 0x8000) != 0
}

#[cfg(not(target_os = "windows"))]
fn is_escape_pressed() -> bool {
    false
}

#[cfg(target_os = "windows")]
fn is_point_likely_window_title_bar(hwnd_raw: isize, point: PhysicalPosition<i32>) -> bool {
    if hwnd_raw == 0 {
        return false;
    }

    let hwnd = hwnd_raw as HWND;
    if hwnd.is_null() || unsafe { IsWindow(hwnd) } == 0 {
        return false;
    }

    let mut rect = RECT {
        left: 0,
        top: 0,
        right: 0,
        bottom: 0,
    };
    if unsafe { GetWindowRect(hwnd, &mut rect as *mut RECT) } == 0 {
        return false;
    }

    if point.x < rect.left || point.x >= rect.right || point.y < rect.top || point.y >= rect.bottom
    {
        return false;
    }

    let title_band_height = if is_console_like_window(hwnd_raw) {
        56
    } else {
        42
    };
    point.y < rect.top.saturating_add(title_band_height)
}

#[cfg(not(target_os = "windows"))]
fn is_point_likely_window_title_bar(_hwnd_raw: isize, _point: PhysicalPosition<i32>) -> bool {
    false
}

fn capture_clipboard_snapshot(clipboard: &mut Clipboard) -> ClipboardSnapshot {
    if let Ok(text) = clipboard.get_text() {
        return ClipboardSnapshot::Text(text);
    }

    if let Ok(image) = clipboard.get_image() {
        return ClipboardSnapshot::Image(ClipboardImageSnapshot {
            width: image.width,
            height: image.height,
            bytes: image.bytes.into_owned(),
        });
    }

    ClipboardSnapshot::Empty
}

fn restore_clipboard_snapshot(
    clipboard: &mut Clipboard,
    snapshot: &ClipboardSnapshot,
) -> Result<(), CommandError> {
    match snapshot {
        ClipboardSnapshot::Empty => Ok(()),
        ClipboardSnapshot::Text(text) => clipboard
            .set_text(text.clone())
            .map_err(|error| CommandError::Clipboard(error.to_string())),
        ClipboardSnapshot::Image(image) => clipboard
            .set_image(ImageData {
                width: image.width,
                height: image.height,
                bytes: Cow::Owned(image.bytes.clone()),
            })
            .map_err(|error| CommandError::Clipboard(error.to_string())),
    }
}

fn read_clipboard_text_trimmed() -> Option<String> {
    let mut clipboard = Clipboard::new().ok()?;
    let value = clipboard.get_text().ok()?;
    let trimmed = value.trim().to_string();
    if trimmed.is_empty() {
        None
    } else {
        Some(trimmed)
    }
}

fn write_clipboard_text_with_retry(value: &str) -> Result<(), CommandError> {
    let mut last_error: Option<String> = None;
    for _ in 0..20 {
        let mut clipboard = match Clipboard::new() {
            Ok(instance) => instance,
            Err(error) => {
                last_error = Some(error.to_string());
                std::thread::sleep(Duration::from_millis(12));
                continue;
            }
        };

        match clipboard.set_text(value.to_string()) {
            Ok(()) => return Ok(()),
            Err(arboard::Error::ClipboardOccupied) => {
                last_error = Some("ClipboardOccupied".to_string());
                std::thread::sleep(Duration::from_millis(12));
            }
            Err(error) => {
                last_error = Some(error.to_string());
                std::thread::sleep(Duration::from_millis(12));
            }
        }
    }

    Err(CommandError::Clipboard(last_error.unwrap_or_else(|| {
        "failed to write clipboard text".to_string()
    })))
}

#[cfg(target_os = "windows")]
fn try_capture_console_selection_for_copy(hwnd_raw: isize, fallback_text: &str) -> Option<String> {
    if hwnd_raw == 0 || !is_console_like_window(hwnd_raw) {
        return None;
    }

    let before_sequence = clipboard_sequence_number();
    if send_console_copy_shortcut(hwnd_raw).is_err() {
        return None;
    }

    for _ in 0..8 {
        std::thread::sleep(Duration::from_millis(10));
        let Some(candidate) = read_clipboard_text_trimmed() else {
            continue;
        };
        if candidate.is_empty() {
            continue;
        }

        let sequence_changed = clipboard_sequence_number() != before_sequence;
        if sequence_changed || candidate != fallback_text {
            return Some(candidate);
        }
    }

    None
}

#[cfg(not(target_os = "windows"))]
fn try_capture_console_selection_for_copy(
    _hwnd_raw: isize,
    _fallback_text: &str,
) -> Option<String> {
    None
}

#[cfg(target_os = "windows")]
fn clipboard_sequence_number() -> u32 {
    unsafe { GetClipboardSequenceNumber() }
}

#[cfg(not(target_os = "windows"))]
fn clipboard_sequence_number() -> u32 {
    0
}

fn capture_console_selection_without_shortcut(
    hwnd_raw: isize,
    before_text: Option<&str>,
) -> Option<(String, bool)> {
    let normalized_before = before_text.map(|value| value.trim()).unwrap_or("");
    let before_sequence = clipboard_sequence_number();

    // Some terminals update clipboard a little after mouse release.
    // Poll briefly to avoid falling back to synthetic copy shortcuts.
    for _ in 0..12 {
        if let Some(after_text) = read_clipboard_text_trimmed() {
            if normalized_before != after_text {
                return Some((after_text, true));
            }
        }
        std::thread::sleep(Duration::from_millis(9));
    }

    if let Some(captured) = try_capture_console_selection_for_copy(hwnd_raw, normalized_before) {
        return Some((captured, true));
    }

    if let Some(after_text) = read_clipboard_text_trimmed() {
        if clipboard_sequence_number() != before_sequence || after_text != normalized_before {
            return Some((after_text, true));
        }
    }

    None
}

fn capture_selected_text_once(hwnd_raw: isize) -> Result<(String, bool), CommandError> {
    let mut clipboard =
        Clipboard::new().map_err(|error| CommandError::Clipboard(error.to_string()))?;
    let snapshot = capture_clipboard_snapshot(&mut clipboard);
    let before_text = match &snapshot {
        ClipboardSnapshot::Text(text) => Some(text.trim().to_string()),
        _ => None,
    };
    let before_sequence = clipboard_sequence_number();

    send_system_copy_shortcut(hwnd_raw)?;
    let mut text = String::new();
    let mut changed = false;
    for _ in 0..20 {
        std::thread::sleep(Duration::from_millis(18));
        let candidate = match clipboard.get_text() {
            Ok(value) => value.trim().to_string(),
            Err(arboard::Error::ContentNotAvailable) => continue,
            Err(arboard::Error::ClipboardOccupied) => continue,
            Err(_) => continue,
        };
        if candidate.is_empty() {
            continue;
        }

        let sequence_changed = clipboard_sequence_number() != before_sequence;
        let text_changed = before_text
            .as_ref()
            .map(|previous| previous != &candidate)
            .unwrap_or(true);

        text = candidate;
        changed = sequence_changed || text_changed;
        if changed {
            break;
        }
    }
    let _ = restore_clipboard_snapshot(&mut clipboard, &snapshot);

    if text.is_empty() {
        return Err(CommandError::Settings(
            "No selectable text captured".to_string(),
        ));
    }

    Ok((text, changed))
}

fn is_link_text(content: &str) -> bool {
    let value = content.trim().to_ascii_lowercase();
    value.starts_with("http://")
        || value.starts_with("https://")
        || value.starts_with("ftp://")
        || value.starts_with("mailto:")
}

fn build_text_entry(content: String) -> ClipboardEntry {
    let kind = if is_link_text(&content) {
        ClipboardKind::Link
    } else {
        ClipboardKind::Text
    };

    ClipboardEntry {
        id: now_id(),
        kind,
        content,
        image_data_url: None,
        copied_at: Utc::now(),
        pinned: false,
    }
}

fn build_image_entry(image: ImageData<'static>) -> Result<ClipboardEntry, CommandError> {
    let width = image.width as u32;
    let height = image.height as u32;
    let bytes = image.bytes.into_owned();

    let rgba = RgbaImage::from_raw(width, height, bytes).ok_or_else(|| {
        CommandError::InvalidImage("Invalid RGBA bytes for clipboard image".to_string())
    })?;

    let mut cursor = Cursor::new(Vec::new());
    DynamicImage::ImageRgba8(rgba)
        .write_to(&mut cursor, ImageFormat::Png)
        .map_err(|error| CommandError::InvalidImage(error.to_string()))?;

    let encoded = BASE64.encode(cursor.into_inner());
    let data_url = format!("data:image/png;base64,{encoded}");

    Ok(ClipboardEntry {
        id: now_id(),
        kind: ClipboardKind::Image,
        content: format!("Image {}x{}", width, height),
        image_data_url: Some(data_url),
        copied_at: Utc::now(),
        pinned: false,
    })
}

fn entry_matches(a: &ClipboardEntry, b: &ClipboardEntry) -> bool {
    if a.kind != b.kind {
        return false;
    }

    if a.kind == ClipboardKind::Image {
        return a.image_data_url == b.image_data_url;
    }

    a.content == b.content
}

fn entry_signature(entry: &ClipboardEntry) -> u64 {
    let mut hasher = std::collections::hash_map::DefaultHasher::new();
    entry.kind.hash(&mut hasher);
    match entry.kind {
        ClipboardKind::Image => entry
            .image_data_url
            .as_deref()
            .unwrap_or("")
            .hash(&mut hasher),
        ClipboardKind::Text | ClipboardKind::Link => entry.content.hash(&mut hasher),
    }
    hasher.finish()
}

fn collect_history(history: &VecDeque<ClipboardEntry>) -> Vec<ClipboardEntry> {
    history.iter().cloned().collect::<Vec<_>>()
}

fn apply_entry_to_clipboard(
    clipboard: &mut Clipboard,
    target: &ClipboardEntry,
) -> Result<(), CommandError> {
    match target.kind {
        ClipboardKind::Image => {
            let data_url = target
                .image_data_url
                .clone()
                .ok_or_else(|| CommandError::InvalidImage("Missing image payload".to_string()))?;
            let image_data = to_image_data(&data_url)?;
            clipboard
                .set_image(image_data)
                .map_err(|error| CommandError::Clipboard(error.to_string()))?;
        }
        ClipboardKind::Text | ClipboardKind::Link => {
            clipboard
                .set_text(target.content.clone())
                .map_err(|error| CommandError::Clipboard(error.to_string()))?;
        }
    }

    Ok(())
}

fn trim_history(history: &mut VecDeque<ClipboardEntry>, max_items: usize) {
    while history.len() > max_items {
        if let Some(index) = history.iter().rposition(|entry| !entry.pinned) {
            history.remove(index);
        } else {
            break;
        }
    }
}

fn normalize_history_order(history: &mut VecDeque<ClipboardEntry>) {
    let mut items = history.make_contiguous().to_vec();
    items.sort_by_key(|item| std::cmp::Reverse(item.copied_at));
    *history = VecDeque::from(items);
}

fn insert_or_promote(
    history: &mut VecDeque<ClipboardEntry>,
    mut incoming: ClipboardEntry,
    max_items: usize,
    dedupe: bool,
) {
    incoming.copied_at = Utc::now();

    if dedupe {
        if let Some(idx) = history
            .iter()
            .position(|entry| entry_matches(entry, &incoming))
        {
            if let Some(mut existing) = history.remove(idx) {
                existing.copied_at = Utc::now();
                existing.pinned = existing.pinned || incoming.pinned;
                history.push_front(existing);
            }
        } else {
            history.push_front(incoming);
        }
    } else {
        history.push_front(incoming);
    }

    trim_history(history, max_items);
    normalize_history_order(history);
}

fn capture_kind_allowed(kind: ClipboardKind, settings: &HistorySettings) -> bool {
    match kind {
        ClipboardKind::Text => settings.capture_text,
        ClipboardKind::Link => settings.capture_link,
        ClipboardKind::Image => settings.capture_image,
    }
}

fn target_position_with_memory<R: Runtime>(
    app: &AppHandle<R>,
    remember_position: bool,
    saved_x: Option<i32>,
    saved_y: Option<i32>,
    width: u32,
    height: u32,
) -> Option<PhysicalPosition<i32>> {
    let follow_pointer = || -> Option<PhysicalPosition<i32>> {
        let pointer = current_pointer_position()?;
        let candidate =
            PhysicalPosition::new(pointer.x.saturating_add(14), pointer.y.saturating_add(16));
        let monitor = monitor_for_point(app, pointer)?;
        Some(clamp_position_to_monitor(
            &monitor, candidate, width, height,
        ))
    };

    if remember_position {
        if let (Some(x), Some(y)) = (saved_x, saved_y) {
            let candidate = PhysicalPosition::new(x, y);
            let monitor =
                monitor_for_point(app, candidate).or_else(|| app.primary_monitor().ok().flatten());
            return monitor
                .map(|monitor| clamp_position_to_monitor(&monitor, candidate, width, height))
                .or(Some(candidate));
        }
    }

    follow_pointer()
}

fn main_window_target_position<R: Runtime>(
    app: &AppHandle<R>,
    settings: &AppSettings,
) -> Option<PhysicalPosition<i32>> {
    let width = clamp_main_window_width(
        settings
            .main_window_width
            .unwrap_or(DEFAULT_MAIN_WINDOW_WIDTH),
    );
    let height = clamp_main_window_height(
        settings
            .main_window_height
            .unwrap_or(DEFAULT_MAIN_WINDOW_HEIGHT),
    );

    target_position_with_memory(
        app,
        settings.window.remember_position,
        settings.main_window_x,
        settings.main_window_y,
        width,
        height,
    )
}

fn apply_main_window_position<R: Runtime>(app: &AppHandle<R>) {
    let Some(settings_state) = app.try_state::<AppSettingsState>() else {
        return;
    };

    let snapshot = match settings_state.data.lock() {
        Ok(settings) => settings.clone(),
        Err(_) => return,
    };

    let Some(position) = main_window_target_position(app, &snapshot) else {
        return;
    };

    if let Some(window) = app.get_webview_window(MAIN_WINDOW_LABEL) {
        let _ = window.set_position(Position::Physical(position));
    }

    if snapshot.window.remember_position
        && (snapshot.main_window_x != Some(position.x)
            || snapshot.main_window_y != Some(position.y))
    {
        if let Ok(mut settings) = settings_state.data.lock() {
            settings.main_window_x = Some(position.x);
            settings.main_window_y = Some(position.y);
        }
        let _ = persist_settings_state(&settings_state);
    }
}

fn selection_bar_width_for_settings(settings: &AppSettings) -> u32 {
    const GAP_NORMAL: u32 = 4;
    const GAP_COMPACT: u32 = 3;
    const SHELL_PADDING_NORMAL: u32 = 22;
    const SHELL_PADDING_COMPACT: u32 = 18;
    const BRAND_WIDTH: u32 = 16;
    const ITEM_COMPACT_WIDTH: u32 = 26;
    const ITEM_ICON_WIDTH: u32 = 13;
    const ITEM_TEXT_GAP: u32 = 4;
    const ITEM_PADDING_X: u32 = 18;
    const WIDTH_SAFETY_BUFFER: u32 = 64;
    let compact_mode = settings.selection_assistant.compact_mode;

    let estimate_label_width = |label: &str| -> u32 {
        let mut width = 0f32;
        for ch in label.chars() {
            width += if is_cjk_char(ch) {
                11.4
            } else if ch.is_ascii_uppercase() {
                7.6
            } else if ch.is_ascii_lowercase() {
                6.8
            } else if ch.is_ascii_digit() {
                6.5
            } else if ch.is_ascii_whitespace() {
                3.8
            } else {
                7.8
            };
        }
        width.ceil() as u32
    };

    let item_width_for_label = |label: &str| -> u32 {
        if compact_mode {
            return ITEM_COMPACT_WIDTH;
        }
        let label_width = estimate_label_width(label).clamp(12, 128);
        ITEM_PADDING_X
            .saturating_add(ITEM_ICON_WIDTH)
            .saturating_add(ITEM_TEXT_GAP)
            .saturating_add(label_width)
    };

    let order =
        normalize_selection_bar_order(settings.agents.bar_order.clone(), &settings.agents.custom);
    let mut action_count = 0u32;
    let mut action_width = 0u32;

    for item in order {
        if !item.enabled {
            continue;
        }

        if is_builtin_selection_bar_key(&item.key) {
            action_count = action_count.saturating_add(1);
            let label = match item.key.as_str() {
                "copy" => "复制",
                "summarize" => "总结",
                "polish" => "优化",
                "explain" => "解释",
                "translate" => "翻译",
                "search" => "搜索",
                _ => "",
            };
            action_width = action_width.saturating_add(item_width_for_label(label));
            continue;
        }

        let Some(custom_id) = parse_custom_selection_bar_key(&item.key) else {
            continue;
        };
        let Some(agent) = settings
            .agents
            .custom
            .iter()
            .find(|entry| entry.id == custom_id)
        else {
            continue;
        };
        action_count = action_count.saturating_add(1);
        action_width = action_width.saturating_add(item_width_for_label(&agent.name));
    }

    let item_count = 1u32.saturating_add(action_count);
    let gap = if compact_mode {
        GAP_COMPACT
    } else {
        GAP_NORMAL
    };
    let shell_padding = if compact_mode {
        SHELL_PADDING_COMPACT
    } else {
        SHELL_PADDING_NORMAL
    };
    let gap_width = item_count.saturating_sub(1).saturating_mul(gap);
    let estimated = BRAND_WIDTH
        .saturating_add(action_width)
        .saturating_add(gap_width)
        .saturating_add(shell_padding)
        .saturating_add(WIDTH_SAFETY_BUFFER);

    if compact_mode {
        estimated.clamp(148, 760)
    } else {
        estimated.clamp(220, 1600)
    }
}

fn selection_bar_target_position<R: Runtime>(
    app: &AppHandle<R>,
    x: i32,
    y: i32,
    width: u32,
    height: u32,
) -> PhysicalPosition<i32> {
    let pointer = PhysicalPosition::new(x, y);
    let candidate = PhysicalPosition::new(x.saturating_add(8), y.saturating_add(10));
    if let Some(monitor) = monitor_for_point(app, pointer) {
        clamp_position_to_monitor(&monitor, candidate, width, height)
    } else {
        candidate
    }
}

fn emit_selection_detected<R: Runtime>(app: &AppHandle<R>, payload: SelectionDetectedPayload) {
    let _ = app.emit(SELECTION_DETECTED_EVENT, payload);
}

fn emit_selection_result<R: Runtime>(app: &AppHandle<R>, payload: SelectionResultPayload) {
    let _ = app.emit(SELECTION_RESULT_UPDATED_EVENT, payload);
}

fn emit_selection_error<R: Runtime>(app: &AppHandle<R>, message: &str) {
    let _ = app.emit(SELECTION_ERROR_EVENT, message.to_string());
}

fn emit_ocr_capture_started<R: Runtime>(app: &AppHandle<R>) {
    let _ = app.emit(OCR_CAPTURE_STARTED_EVENT, true);
}

fn emit_ocr_capture_canceled<R: Runtime>(app: &AppHandle<R>) {
    let _ = app.emit(OCR_CAPTURE_CANCELED_EVENT, true);
}

fn emit_ocr_result<R: Runtime>(app: &AppHandle<R>, payload: OcrResultPayload) {
    let _ = app.emit(OCR_RESULT_UPDATED_EVENT, payload);
}

fn emit_ocr_error<R: Runtime>(app: &AppHandle<R>, message: &str) {
    let _ = app.emit(OCR_ERROR_EVENT, message.to_string());
}

fn emit_history_updated<R: Runtime>(app: &AppHandle<R>, items: &[ClipboardEntry]) {
    let _ = app.emit(HISTORY_UPDATED_EVENT, items.to_vec());
}

fn window_background_color_for_theme(theme: ThemePreset) -> Color {
    match theme {
        ThemePreset::Blue => Color(18, 28, 45, 255),
        ThemePreset::DeepBlack => Color(10, 10, 10, 255),
        ThemePreset::Gray => Color(38, 40, 44, 255),
        ThemePreset::White => Color(255, 255, 255, 255),
    }
}

#[cfg(target_os = "windows")]
fn apply_native_result_window_corner_preference<R: Runtime>(window: &tauri::WebviewWindow<R>) {
    let Ok(hwnd) = window.hwnd() else {
        return;
    };
    // Use native rounded window corners for stable rendering on Windows.
    let preference: i32 = DWMWCP_ROUND;
    unsafe {
        let _ = DwmSetWindowAttribute(
            hwnd.0 as HWND,
            DWMWA_WINDOW_CORNER_PREFERENCE as u32,
            &preference as *const _ as *const core::ffi::c_void,
            std::mem::size_of::<i32>() as u32,
        );
    }
}

#[cfg(not(target_os = "windows"))]
fn apply_native_result_window_corner_preference<R: Runtime>(_window: &tauri::WebviewWindow<R>) {}

fn apply_result_windows_background<R: Runtime>(app: &AppHandle<R>, settings: &AppSettings) {
    let color = Some(window_background_color_for_theme(settings.theme_preset));
    if let Some(window) = app.get_webview_window(SELECTION_RESULT_WINDOW_LABEL) {
        let _ = window.set_background_color(color);
        apply_native_result_window_corner_preference(&window);
    }
    if let Some(window) = app.get_webview_window(OCR_RESULT_WINDOW_LABEL) {
        let _ = window.set_background_color(color);
        apply_native_result_window_corner_preference(&window);
    }
}

fn show_selection_bar_window<R: Runtime>(
    app: &AppHandle<R>,
    payload: &SelectionDetectedPayload,
) -> Result<(), CommandError> {
    let Some(window) = app.get_webview_window(SELECTION_BAR_WINDOW_LABEL) else {
        return Err(CommandError::Settings(
            "Selection bar window not found".to_string(),
        ));
    };

    let mut width = app
        .try_state::<AppSettingsState>()
        .and_then(|settings_state| {
            settings_state
                .data
                .lock()
                .ok()
                .map(|settings| selection_bar_width_for_settings(&settings))
        })
        .unwrap_or(DEFAULT_SELECTION_BAR_WIDTH);
    let height = SELECTION_BAR_HEIGHT;

    // Keep width within current monitor logical bounds to prevent any horizontal overflow.
    let pointer = PhysicalPosition::new(payload.x, payload.y);
    if let Some(monitor) = monitor_for_point(app, pointer) {
        let monitor_width_px = monitor.size().width;
        let monitor_logical_width = ((monitor_width_px as f64) / monitor.scale_factor().max(0.1))
            .floor()
            .clamp(0.0, u32::MAX as f64) as u32;
        let max_safe_width = monitor_logical_width.saturating_sub(12).max(180);
        width = width.min(max_safe_width);
    }

    // Keep terminal/native selection highlight: show bar without activating it first.
    let _ = window.set_focusable(false);
    let _ = window.set_size(Size::Logical(LogicalSize::new(width as f64, height as f64)));
    let target = selection_bar_target_position(app, payload.x, payload.y, width, height);
    let _ = window.set_position(Position::Physical(target));
    let _ = window.show();
    let app_handle = app.clone();
    std::thread::spawn(move || {
        std::thread::sleep(Duration::from_millis(80));
        if let Some(window) = app_handle.get_webview_window(SELECTION_BAR_WINDOW_LABEL) {
            let _ = window.set_focusable(true);
        }
    });
    Ok(())
}

fn hide_selection_bar_window<R: Runtime>(app: &AppHandle<R>) {
    if let Some(window) = app.get_webview_window(SELECTION_BAR_WINDOW_LABEL) {
        let _ = window.hide();
    }
}

fn show_selection_result_window<R: Runtime>(app: &AppHandle<R>) -> Result<(), CommandError> {
    let Some(settings_state) = app.try_state::<AppSettingsState>() else {
        return Err(CommandError::Settings(
            "Settings state unavailable".to_string(),
        ));
    };
    let settings_snapshot = settings_state
        .data
        .lock()
        .map(|settings| settings.clone())
        .unwrap_or_else(|_| AppSettings::default());
    let always_on_top = settings_snapshot
        .selection_assistant
        .result_window_always_on_top;

    let Some(window) = app.get_webview_window(SELECTION_RESULT_WINDOW_LABEL) else {
        return Err(CommandError::Settings(
            "Selection result window not found".to_string(),
        ));
    };

    let target_width = clamp_selection_result_window_width(
        settings_snapshot
            .selection_result_window_width
            .unwrap_or(DEFAULT_SELECTION_RESULT_WINDOW_WIDTH),
    );
    let target_height = clamp_selection_result_window_height(
        settings_snapshot
            .selection_result_window_height
            .unwrap_or(DEFAULT_SELECTION_RESULT_WINDOW_HEIGHT),
    );

    if settings_snapshot.window.remember_main_window_size {
        let _ = window.set_size(Size::Logical(LogicalSize::new(
            target_width as f64,
            target_height as f64,
        )));
    }

    if let Some(target_position) = target_position_with_memory(
        app,
        settings_snapshot
            .selection_assistant
            .remember_result_window_position,
        settings_snapshot.selection_result_window_x,
        settings_snapshot.selection_result_window_y,
        target_width,
        target_height,
    ) {
        let _ = window.set_position(Position::Physical(target_position));
        if settings_snapshot
            .selection_assistant
            .remember_result_window_position
            && (settings_snapshot.selection_result_window_x != Some(target_position.x)
                || settings_snapshot.selection_result_window_y != Some(target_position.y))
        {
            if let Ok(mut settings) = settings_state.data.lock() {
                settings.selection_result_window_x = Some(target_position.x);
                settings.selection_result_window_y = Some(target_position.y);
            }
            let _ = persist_settings_state(&settings_state);
        }
    }

    let _ = window.set_always_on_top(always_on_top);
    let _ = window.set_background_color(Some(window_background_color_for_theme(
        settings_snapshot.theme_preset,
    )));
    let _ = window.show();
    let _ = window.set_focus();
    Ok(())
}

fn show_ocr_capture_window<R: Runtime>(app: &AppHandle<R>) -> Result<(), CommandError> {
    let Some(window) = app.get_webview_window(OCR_CAPTURE_WINDOW_LABEL) else {
        return Err(CommandError::Settings(
            "OCR capture window not found".to_string(),
        ));
    };

    let pointer = current_pointer_position().unwrap_or(PhysicalPosition::new(40, 40));
    if let Some(monitor) =
        monitor_for_point(app, pointer).or_else(|| app.primary_monitor().ok().flatten())
    {
        let monitor_pos = monitor.position();
        let monitor_size = monitor.size();
        let _ = window.set_position(Position::Physical(*monitor_pos));
        let _ = window.set_size(Size::Physical(tauri::PhysicalSize::new(
            monitor_size.width,
            monitor_size.height,
        )));
    }

    let _ = window.set_always_on_top(true);
    let _ = window.show();
    Ok(())
}

fn hide_ocr_capture_window<R: Runtime>(app: &AppHandle<R>) {
    if let Some(window) = app.get_webview_window(OCR_CAPTURE_WINDOW_LABEL) {
        let _ = window.hide();
    }
}

fn show_ocr_result_window<R: Runtime>(app: &AppHandle<R>) -> Result<(), CommandError> {
    let Some(settings_state) = app.try_state::<AppSettingsState>() else {
        return Err(CommandError::Settings(
            "Settings state unavailable".to_string(),
        ));
    };
    let settings_snapshot = settings_state
        .data
        .lock()
        .map(|settings| settings.clone())
        .unwrap_or_else(|_| AppSettings::default());
    let always_on_top = settings_snapshot.ocr.result_window_always_on_top;

    let Some(window) = app.get_webview_window(OCR_RESULT_WINDOW_LABEL) else {
        return Err(CommandError::Settings(
            "OCR result window not found".to_string(),
        ));
    };

    let target_width = clamp_ocr_result_window_width(
        settings_snapshot
            .ocr_result_window_width
            .unwrap_or(DEFAULT_OCR_RESULT_WINDOW_WIDTH),
    );
    let target_height = clamp_ocr_result_window_height(
        settings_snapshot
            .ocr_result_window_height
            .unwrap_or(DEFAULT_OCR_RESULT_WINDOW_HEIGHT),
    );

    if settings_snapshot.window.remember_main_window_size {
        let _ = window.set_size(Size::Logical(LogicalSize::new(
            target_width as f64,
            target_height as f64,
        )));
    }

    if let Some(target_position) = target_position_with_memory(
        app,
        settings_snapshot.ocr.remember_result_window_position,
        settings_snapshot.ocr_result_window_x,
        settings_snapshot.ocr_result_window_y,
        target_width,
        target_height,
    ) {
        let _ = window.set_position(Position::Physical(target_position));
        if settings_snapshot.ocr.remember_result_window_position
            && (settings_snapshot.ocr_result_window_x != Some(target_position.x)
                || settings_snapshot.ocr_result_window_y != Some(target_position.y))
        {
            if let Ok(mut settings) = settings_state.data.lock() {
                settings.ocr_result_window_x = Some(target_position.x);
                settings.ocr_result_window_y = Some(target_position.y);
            }
            let _ = persist_settings_state(&settings_state);
        }
    }

    let _ = window.set_always_on_top(always_on_top);
    let _ = window.set_background_color(Some(window_background_color_for_theme(
        settings_snapshot.theme_preset,
    )));
    let _ = window.show();
    let _ = window.set_focus();
    Ok(())
}

fn start_ocr_capture_workflow<R: Runtime>(app: &AppHandle<R>) -> Result<(), CommandError> {
    let Some(settings_state) = app.try_state::<AppSettingsState>() else {
        return Err(CommandError::Settings(
            "Settings state unavailable".to_string(),
        ));
    };
    let enabled = settings_state
        .data
        .lock()
        .map(|settings| settings.ocr.enabled)
        .unwrap_or(false);
    if !enabled {
        return Err(CommandError::Settings(
            "请先在设置中启用智能 OCR".to_string(),
        ));
    }

    let Some(ocr_runtime) = app.try_state::<OcrRuntimeState>() else {
        return Err(CommandError::Settings(
            "OCR runtime state unavailable".to_string(),
        ));
    };
    ocr_runtime.capture_active.store(true, Ordering::Relaxed);
    ocr_runtime.suppress_blur_until_ms.store(
        now_epoch_millis().saturating_add(OCR_CAPTURE_BLUR_SUPPRESS_MS),
        Ordering::Relaxed,
    );
    if let Err(error) = refresh_ocr_capture_snapshot(&ocr_runtime) {
        eprintln!("[OCR] preload capture snapshot failed: {error}");
        if let Ok(mut snapshot) = ocr_runtime.capture_snapshot.lock() {
            *snapshot = None;
        }
    }
    show_ocr_capture_window(app)?;
    emit_ocr_capture_started(app);
    Ok(())
}

fn open_in_default_browser(url: &str) -> Result<(), CommandError> {
    #[cfg(target_os = "windows")]
    {
        Command::new("cmd")
            .args(["/C", "start", "", url])
            .spawn()
            .map_err(|error| CommandError::Settings(error.to_string()))?;
        Ok(())
    }

    #[cfg(not(target_os = "windows"))]
    {
        let _ = url;
        Err(CommandError::Settings(
            "Open browser is currently implemented for Windows only".to_string(),
        ))
    }
}

fn resolve_tts_voice(
    tts: &TtsSettings,
    language_hint: Option<&str>,
    voice_override: Option<&str>,
    text: &str,
) -> String {
    if let Some(override_voice) = voice_override {
        let trimmed = override_voice.trim();
        if !trimmed.is_empty() {
            return trimmed.to_string();
        }
    }

    let mut cjk_count = 0usize;
    let mut latin_count = 0usize;
    for ch in text.chars().take(1200) {
        if ('\u{4E00}'..='\u{9FFF}').contains(&ch)
            || ('\u{3400}'..='\u{4DBF}').contains(&ch)
            || ('\u{3040}'..='\u{30FF}').contains(&ch)
            || ('\u{AC00}'..='\u{D7AF}').contains(&ch)
        {
            cjk_count = cjk_count.saturating_add(1);
        } else if ch.is_ascii_alphabetic() {
            latin_count = latin_count.saturating_add(1);
        }
    }
    if cjk_count > 0 && cjk_count >= latin_count / 2 {
        return tts.voice_zh_cn.clone();
    }
    if latin_count > 0 {
        return tts.voice_en_us.clone();
    }

    let hint = language_hint
        .unwrap_or_default()
        .trim()
        .to_ascii_lowercase();
    if hint.starts_with("zh") {
        tts.voice_zh_cn.clone()
    } else {
        tts.voice_en_us.clone()
    }
}

fn format_tts_rate_arg(rate_percent: i32) -> String {
    let clamped = rate_percent.clamp(MIN_TTS_RATE_PERCENT, MAX_TTS_RATE_PERCENT);
    if clamped >= 0 {
        format!("+{}%", clamped)
    } else {
        format!("{}%", clamped)
    }
}

fn configure_background_command(command: &mut Command) {
    #[cfg(target_os = "windows")]
    {
        command.creation_flags(CREATE_NO_WINDOW);
    }
}

fn run_edge_tts_process(
    executable: &str,
    prefix_args: &[&str],
    voice: &str,
    rate: &str,
    text: &str,
    output_path: &Path,
) -> Result<(), String> {
    let mut args = prefix_args
        .iter()
        .map(|item| (*item).to_string())
        .collect::<Vec<_>>();
    args.extend([
        "--voice".to_string(),
        voice.to_string(),
        "--rate".to_string(),
        rate.to_string(),
        "--text".to_string(),
        text.to_string(),
        "--write-media".to_string(),
        output_path.to_string_lossy().into_owned(),
    ]);

    let mut command = Command::new(executable);
    configure_background_command(&mut command);
    let output = command
        .args(args)
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::piped())
        .output()
        .map_err(|error| format!("{executable}: {error}"))?;

    if output.status.success() {
        return Ok(());
    }

    let stderr = String::from_utf8_lossy(&output.stderr).trim().to_string();
    if stderr.is_empty() {
        Err(format!(
            "{executable}: exited with status {}",
            output.status
        ))
    } else {
        Err(format!("{executable}: {stderr}"))
    }
}

fn is_missing_edge_tts_error(message: &str) -> bool {
    let lowered = message.to_ascii_lowercase();
    lowered.contains("no module named edge_tts")
        || lowered.contains("is not recognized")
        || lowered.contains("not recognized as")
        || lowered.contains("not found")
        || lowered.contains("cannot find")
        || lowered.contains("no such file")
}

fn has_edge_tts_runtime() -> bool {
    let probes: [(&str, &[&str]); 3] = [
        ("edge-tts", &["--help"]),
        ("python", &["-m", "edge_tts", "--help"]),
        ("py", &["-m", "edge_tts", "--help"]),
    ];

    for (executable, args) in probes {
        let mut command = Command::new(executable);
        configure_background_command(&mut command);
        let output = command
            .args(args)
            .stdin(Stdio::null())
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .output();
        if output
            .map(|result| result.status.success())
            .unwrap_or(false)
        {
            return true;
        }
    }

    false
}

fn try_auto_install_edge_tts() -> Result<(), String> {
    let mut errors = Vec::new();
    for executable in ["python", "py"] {
        let mut command = Command::new(executable);
        configure_background_command(&mut command);
        let output = match command
            .args([
                "-m",
                "pip",
                "install",
                "edge-tts",
                "--disable-pip-version-check",
            ])
            .stdin(Stdio::null())
            .stdout(Stdio::null())
            .stderr(Stdio::piped())
            .output()
        {
            Ok(result) => result,
            Err(error) => {
                errors.push(format!("{executable}: {error}"));
                continue;
            }
        };

        if output.status.success() {
            return Ok(());
        }

        let stderr = String::from_utf8_lossy(&output.stderr).trim().to_string();
        if stderr.is_empty() {
            errors.push(format!(
                "{executable}: pip install exited with {}",
                output.status
            ));
        } else {
            errors.push(format!("{executable}: {stderr}"));
        }
    }

    if errors.is_empty() {
        Err("未找到可用的 Python/pip 环境".to_string())
    } else {
        Err(errors.join(" | "))
    }
}

fn try_auto_install_edge_tts_with_lock() -> Result<(), String> {
    if EDGE_TTS_AUTO_INSTALL_ATTEMPTED
        .compare_exchange(false, true, Ordering::AcqRel, Ordering::Acquire)
        .is_err()
    {
        return Err(EDGE_TTS_INSTALL_IN_PROGRESS.to_string());
    }

    match try_auto_install_edge_tts() {
        Ok(()) => Ok(()),
        Err(error) => {
            EDGE_TTS_AUTO_INSTALL_ATTEMPTED.store(false, Ordering::Release);
            Err(error)
        }
    }
}

fn execute_tts_attempt(
    attempt_errors: &mut Vec<String>,
    cmd: &str,
    prefix: &[&str],
    voice: &str,
    rate: &str,
    text: &str,
    output_path: &Path,
) -> bool {
    match run_edge_tts_process(cmd, prefix, voice, rate, text, output_path) {
        Ok(_) => true,
        Err(error) => {
            attempt_errors.push(error);
            false
        }
    }
}

fn synthesize_tts_audio(
    tts: &TtsSettings,
    text: &str,
    language_hint: Option<&str>,
    voice_override: Option<&str>,
) -> Result<SynthesizeTtsResult, CommandError> {
    let text = text.trim();
    if text.is_empty() {
        return Err(CommandError::Settings("TTS 文本为空".to_string()));
    }

    if text.chars().count() > MAX_TTS_TEXT_CHARS {
        return Err(CommandError::Settings(format!(
            "TTS 文本过长（最多 {MAX_TTS_TEXT_CHARS} 个字符）"
        )));
    }

    let voice = resolve_tts_voice(tts, language_hint, voice_override, text);
    let rate = format_tts_rate_arg(tts.rate_percent);
    let mut output_path = std::env::temp_dir();
    output_path.push(format!("snapparse-tts-{}.mp3", now_id()));

    let mut attempt_errors = Vec::new();

    let mut succeeded = match tts.runtime_mode {
        TtsRuntimeMode::EdgeCliOnly => execute_tts_attempt(
            &mut attempt_errors,
            "edge-tts",
            &[],
            &voice,
            &rate,
            text,
            &output_path,
        ),
        TtsRuntimeMode::PythonModuleOnly => {
            execute_tts_attempt(
                &mut attempt_errors,
                "python",
                &["-m", "edge_tts"],
                &voice,
                &rate,
                text,
                &output_path,
            ) || execute_tts_attempt(
                &mut attempt_errors,
                "py",
                &["-m", "edge_tts"],
                &voice,
                &rate,
                text,
                &output_path,
            )
        }
        TtsRuntimeMode::DualFallback => {
            execute_tts_attempt(
                &mut attempt_errors,
                "edge-tts",
                &[],
                &voice,
                &rate,
                text,
                &output_path,
            ) || execute_tts_attempt(
                &mut attempt_errors,
                "python",
                &["-m", "edge_tts"],
                &voice,
                &rate,
                text,
                &output_path,
            ) || execute_tts_attempt(
                &mut attempt_errors,
                "py",
                &["-m", "edge_tts"],
                &voice,
                &rate,
                text,
                &output_path,
            )
        }
    };

    let missing_dependency = !succeeded
        && !attempt_errors.is_empty()
        && attempt_errors
            .iter()
            .all(|error| is_missing_edge_tts_error(error));
    if missing_dependency {
        match try_auto_install_edge_tts_with_lock() {
            Ok(_) => {
                attempt_errors.clear();
                succeeded = match tts.runtime_mode {
                    TtsRuntimeMode::EdgeCliOnly => execute_tts_attempt(
                        &mut attempt_errors,
                        "edge-tts",
                        &[],
                        &voice,
                        &rate,
                        text,
                        &output_path,
                    ),
                    TtsRuntimeMode::PythonModuleOnly => {
                        execute_tts_attempt(
                            &mut attempt_errors,
                            "python",
                            &["-m", "edge_tts"],
                            &voice,
                            &rate,
                            text,
                            &output_path,
                        ) || execute_tts_attempt(
                            &mut attempt_errors,
                            "py",
                            &["-m", "edge_tts"],
                            &voice,
                            &rate,
                            text,
                            &output_path,
                        )
                    }
                    TtsRuntimeMode::DualFallback => {
                        execute_tts_attempt(
                            &mut attempt_errors,
                            "edge-tts",
                            &[],
                            &voice,
                            &rate,
                            text,
                            &output_path,
                        ) || execute_tts_attempt(
                            &mut attempt_errors,
                            "python",
                            &["-m", "edge_tts"],
                            &voice,
                            &rate,
                            text,
                            &output_path,
                        ) || execute_tts_attempt(
                            &mut attempt_errors,
                            "py",
                            &["-m", "edge_tts"],
                            &voice,
                            &rate,
                            text,
                            &output_path,
                        )
                    }
                };
            }
            Err(error) => {
                if error == EDGE_TTS_INSTALL_IN_PROGRESS {
                    attempt_errors.push("Edge TTS 正在后台初始化，请稍后重试".to_string());
                } else {
                    attempt_errors.push(format!("auto-install failed: {error}"));
                }
            }
        }
    }

    if !succeeded {
        let _ = fs::remove_file(&output_path);
        let detail = if attempt_errors.is_empty() {
            "未知错误".to_string()
        } else {
            attempt_errors.join(" | ")
        };
        return Err(CommandError::Settings(format!(
            "TTS 调用失败，请确认已安装 edge-tts（python -m pip install edge-tts）: {detail}"
        )));
    }

    let bytes_result = fs::read(&output_path);
    let _ = fs::remove_file(&output_path);
    let bytes = bytes_result.map_err(|error| {
        CommandError::Settings(format!(
            "读取 TTS 音频失败（{}）: {}",
            output_path.display(),
            error
        ))
    })?;

    if bytes.is_empty() {
        return Err(CommandError::Settings("TTS 返回空音频".to_string()));
    }

    Ok(SynthesizeTtsResult {
        audio_base64: BASE64.encode(bytes),
        mime_type: "audio/mpeg".to_string(),
        voice_used: voice,
    })
}

fn build_search_url(template: &str, text: &str) -> String {
    let encoded = urlencoding::encode(text);
    if template.contains("{query}") {
        template.replace("{query}", &encoded)
    } else if template.contains("{}") {
        template.replacen("{}", &encoded, 1)
    } else {
        format!("{template}{encoded}")
    }
}

fn choose_builtin_prompt(
    action: SelectionActionKind,
    text: &str,
    translate_from: Option<&str>,
    translate_to: Option<&str>,
    ui_language: &str,
) -> (String, String) {
    fn is_english_ui(language: &str) -> bool {
        language.eq_ignore_ascii_case("en-US")
    }

    fn output_language_label(language: &str) -> &'static str {
        if is_english_ui(language) {
            "English"
        } else {
            "Chinese (Simplified)"
        }
    }

    fn language_label(code: &str) -> Cow<'static, str> {
        match code {
            "auto" => Cow::Borrowed("Auto Detect"),
            "zh-CN" => Cow::Borrowed("Chinese (Simplified)"),
            "en-US" => Cow::Borrowed("English"),
            "ja-JP" => Cow::Borrowed("Japanese"),
            "ko-KR" => Cow::Borrowed("Korean"),
            _ => Cow::Owned(code.to_string()),
        }
    }

    let output_language = output_language_label(ui_language);
    match action {
        SelectionActionKind::Summarize => {
            let system_template = "You are a senior summarization assistant.
Output language: {output_language}.
Requirements:
- Return only the final summary text.
- Preserve key facts, entities, numbers, dates, and technical terms accurately.
- Remove redundancy and keep the summary concise and clear.
- Do not add headings, commentary, or extra explanations unless explicitly requested.";
            let user_prompt = format!(
                "Task: Summarize the content faithfully and concisely.
Text:
{text}"
            );
            (
                system_template.replace("{output_language}", output_language),
                user_prompt,
            )
        }
        SelectionActionKind::Polish => {
            let system_template = "You are a professional writing editor.
Output language: {output_language}.
Requirements:
- Return only the rewritten text.
- Preserve the original meaning, factual details, and intent.
- Improve clarity, grammar, fluency, and structure.
- Keep names, numbers, code snippets, URLs, and quoted text accurate.
- Do not add commentary, labels, or extra sections.";
            let user_prompt = format!(
                "Task: Rewrite and improve the text while preserving meaning.
Text:
{text}"
            );
            (
                system_template.replace("{output_language}", output_language),
                user_prompt,
            )
        }
        SelectionActionKind::Explain => {
            let system_template = "You are a practical programming explainer and problem-solving assistant.
Output language: {output_language}.
Requirements:
- Return only content directly related to the input text.
- Prefer simple and clear language; avoid unnecessary jargon.
- Prioritize code and programming issues when present.
- Use exactly three sections in this order:
  1) Problem Understanding
  2) Why It Happens
  3) Solution Steps
- Section headings must also be in the output language.
- In Solution Steps, provide concrete, low-risk, executable steps.
- If missing key context, state the minimum missing info and continue with best-effort guidance.
- Do not add greetings, disclaimers, or unrelated commentary.";
            let user_prompt = format!(
                "Task: Explain the issue clearly and provide an easy-to-follow solution.
Text:
{text}"
            );
            (
                system_template.replace("{output_language}", output_language),
                user_prompt,
            )
        }
        SelectionActionKind::Translate => {
            let from = translate_from.unwrap_or("auto");
            let to = translate_to.unwrap_or(if is_english_ui(ui_language) {
                "en-US"
            } else {
                "zh-CN"
            });
            let from_display = language_label(from);
            let to_display = language_label(to);
            let system_template = "You are a professional translator.
Target language: {target_language}.
Requirements:
- Return only the translated text.
- Preserve meaning faithfully; do not omit, add, or invent information.
- Keep proper nouns, numbers, code, URLs, and formatting accurate.
- Preserve line breaks and list structure when useful for readability.
- Do not add notes, explanations, or bilingual output unless explicitly requested.";
            let user_prompt = format!(
                "Source language: {from_display}
Target language: {to_display}
Task: Translate the text.
Text:
{text}"
            );
            (
                system_template.replace("{target_language}", &to_display),
                user_prompt,
            )
        }
        SelectionActionKind::Custom => (
            "You are a text processing assistant. Follow the user's instruction and return only the final output.".to_string(),
            format!("Text:\n{text}"),
        ),
    }
}

fn custom_agent_system_prompt() -> &'static str {
    "You are a configurable text-processing assistant.
Follow the user's custom instruction exactly.
Requirements:
- Return only the final processed text.
- Do not add preface, labels, markdown fences, or explanations unless explicitly requested.
- Preserve factual details, names, numbers, code snippets, and URLs accurately.
- Respect the language, tone, and format requested by the user."
}

fn extract_llm_text_content(value: &serde_json::Value) -> String {
    if let Some(text) = value
        .get("choices")
        .and_then(|choices| choices.get(0))
        .and_then(|item| item.get("text"))
        .and_then(|text| text.as_str())
    {
        return text.trim().to_string();
    }

    if let Some(content) = value
        .get("choices")
        .and_then(|choices| choices.get(0))
        .and_then(|item| item.get("message"))
        .and_then(|message| message.get("content"))
    {
        if let Some(text) = content.as_str() {
            return text.trim().to_string();
        }

        if let Some(items) = content.as_array() {
            let mut merged = String::new();
            for item in items {
                if let Some(text) = item.get("text").and_then(|value| value.as_str()) {
                    merged.push_str(text);
                }
            }
            return merged.trim().to_string();
        }
    }

    if let Some(output_text) = value.get("output_text").and_then(|item| item.as_str()) {
        return output_text.trim().to_string();
    }

    if let Some(output_items) = value.get("output").and_then(|item| item.as_array()) {
        let mut merged = String::new();
        for output_item in output_items {
            if let Some(content_items) = output_item.get("content").and_then(|item| item.as_array())
            {
                for content_item in content_items {
                    if let Some(text) = content_item.get("text").and_then(|item| item.as_str()) {
                        merged.push_str(text);
                    }
                }
            }
        }
        if !merged.trim().is_empty() {
            return merged.trim().to_string();
        }
    }

    String::new()
}

fn build_auth_header_value(api_key: &str, prefer_bearer: bool) -> String {
    let trimmed = api_key.trim();
    if trimmed.to_ascii_lowercase().starts_with("bearer ") {
        trimmed.to_string()
    } else if prefer_bearer {
        format!("Bearer {trimmed}")
    } else {
        trimmed.to_string()
    }
}

fn auth_header_candidates(api_key: &str, prefer_bearer: bool) -> Vec<String> {
    let mut candidates = Vec::<String>::new();
    let preferred = build_auth_header_value(api_key, prefer_bearer);
    push_unique_string(&mut candidates, preferred);
    if !api_key.trim().to_ascii_lowercase().starts_with("bearer ") {
        push_unique_string(
            &mut candidates,
            build_auth_header_value(api_key, !prefer_bearer),
        );
    }
    candidates
}

fn should_use_glm_layout_parsing(vision: &VisionSettings) -> bool {
    let base_url = vision.base_url.trim().to_ascii_lowercase();
    let model = vision.model.trim().to_ascii_lowercase();
    base_url.contains("/layout_parsing")
        || base_url.contains("open.bigmodel.cn")
        || model.starts_with("glm-ocr")
}

fn resolve_glm_layout_parsing_endpoint(vision: &VisionSettings) -> String {
    let base_url = vision.base_url.trim();
    let model = vision.model.trim().to_ascii_lowercase();
    if base_url.is_empty()
        || (model.starts_with("glm-ocr")
            && base_url.to_ascii_lowercase().contains("/chat/completions"))
    {
        "https://open.bigmodel.cn/api/paas/v4/layout_parsing".to_string()
    } else {
        base_url.to_string()
    }
}

fn push_unique_string(bucket: &mut Vec<String>, value: String) {
    if value.trim().is_empty() {
        return;
    }
    if bucket.iter().any(|item| item == &value) {
        return;
    }
    bucket.push(value);
}

fn vision_file_payload_candidates(image_data_url: &str) -> Vec<String> {
    let mut candidates = Vec::<String>::new();
    let value = image_data_url.trim();

    if value.starts_with("http://") || value.starts_with("https://") {
        push_unique_string(&mut candidates, value.to_string());
        return candidates;
    }

    // Keep original payload first (e.g. data:image/png;base64,...) so server can infer type.
    push_unique_string(&mut candidates, value.to_string());

    if let Some((prefix, payload)) = value.split_once(',') {
        if prefix.to_ascii_lowercase().contains("base64") {
            push_unique_string(&mut candidates, payload.trim().to_string());
        }
    }

    candidates
}

fn push_candidate_text(raw: &str, bucket: &mut Vec<String>) {
    let trimmed = raw.trim();
    if trimmed.is_empty() {
        return;
    }
    if bucket.iter().any(|item| item == trimmed) {
        return;
    }
    bucket.push(trimmed.to_string());
}

fn collect_ocr_text_candidates(value: &serde_json::Value, bucket: &mut Vec<String>) {
    if let Some(text) = value.as_str() {
        push_candidate_text(text, bucket);
        return;
    }

    if let Some(items) = value.as_array() {
        for item in items {
            collect_ocr_text_candidates(item, bucket);
        }
        return;
    }

    let Some(map) = value.as_object() else {
        return;
    };

    for key in [
        "text",
        "content",
        "markdown",
        "md_results",
        "md_result",
        "ocr_text",
        "full_text",
        "output_text",
    ] {
        if let Some(item) = map.get(key) {
            collect_ocr_text_candidates(item, bucket);
        }
    }
    for key in [
        "result",
        "results",
        "data",
        "pages",
        "blocks",
        "items",
        "layout_details",
        "layout_visualization",
        "paragraphs",
        "lines",
    ] {
        if let Some(item) = map.get(key) {
            collect_ocr_text_candidates(item, bucket);
        }
    }
}

fn strip_markdown_heading_prefix(line: &str) -> Option<&str> {
    let trimmed = line.trim_start();
    let bytes = trimmed.as_bytes();
    let mut idx = 0usize;
    while idx < bytes.len() && idx < 6 && bytes[idx] == b'#' {
        idx += 1;
    }
    if idx == 0 || idx >= bytes.len() || !bytes[idx].is_ascii_whitespace() {
        return None;
    }
    Some(trimmed[idx..].trim_start())
}

fn normalize_ocr_text_output(raw: &str) -> String {
    let lines = raw
        .lines()
        .map(|line| {
            if let Some(rest) = strip_markdown_heading_prefix(line) {
                rest.to_string()
            } else {
                line.to_string()
            }
        })
        .collect::<Vec<_>>();
    lines.join("\n").trim().to_string()
}

fn extract_vision_ocr_text(value: &serde_json::Value) -> String {
    if let Some(markdown) = value.get("md_results").and_then(|item| item.as_str()) {
        let normalized = normalize_ocr_text_output(markdown);
        if !normalized.is_empty() {
            return normalized;
        }
    }

    let mut candidates = Vec::<String>::new();
    collect_ocr_text_candidates(value, &mut candidates);

    let longest = candidates
        .into_iter()
        .max_by_key(|item| item.chars().count())
        .unwrap_or_default();

    if !longest.is_empty() {
        return normalize_ocr_text_output(&longest);
    }

    normalize_ocr_text_output(&extract_llm_text_content(value))
}

async fn call_glm_layout_parsing_ocr(
    client: &reqwest::Client,
    vision: &VisionSettings,
    image_data_url: &str,
) -> Result<String, CommandError> {
    let endpoint = resolve_glm_layout_parsing_endpoint(vision);
    let auth_headers = auth_header_candidates(&vision.api_key, false);
    let file_candidates = vision_file_payload_candidates(image_data_url);
    if file_candidates.is_empty() {
        return Err(CommandError::Settings(
            "OCR image payload is empty".to_string(),
        ));
    }

    let model_name = vision.model.trim();
    let mut last_error: Option<String> = None;
    for file_payload in file_candidates {
        for (auth_index, auth_header) in auth_headers.iter().enumerate() {
            let request_body = json!({
                "model": model_name,
                "file": file_payload
            });

            let mut final_status: Option<reqwest::StatusCode> = None;
            let mut final_value: serde_json::Value = json!({});
            let mut final_raw_body = String::new();

            for attempt in 0..MODEL_REQUEST_MAX_ATTEMPTS {
                let response = match client
                    .post(&endpoint)
                    .timeout(Duration::from_millis(vision.timeout_ms))
                    .header(AUTHORIZATION, auth_header.as_str())
                    .header(CONTENT_TYPE, "application/json")
                    .json(&request_body)
                    .send()
                    .await
                {
                    Ok(resp) => resp,
                    Err(error) => {
                        if should_retry_network_error(&error)
                            && attempt + 1 < MODEL_REQUEST_MAX_ATTEMPTS
                        {
                            last_error = Some(error.to_string());
                            sleep_with_backoff(attempt).await;
                            continue;
                        }
                        return Err(CommandError::Settings(error.to_string()));
                    }
                };

                let status = response.status();
                let raw_body = match read_response_body_lossy(response).await {
                    Ok(value) => value,
                    Err(error) => {
                        if should_retry_network_error(&error)
                            && attempt + 1 < MODEL_REQUEST_MAX_ATTEMPTS
                        {
                            last_error = Some(error.to_string());
                            sleep_with_backoff(attempt).await;
                            continue;
                        }
                        return Err(CommandError::Settings(error.to_string()));
                    }
                };
                let parsed_value = serde_json::from_str::<serde_json::Value>(&raw_body)
                    .unwrap_or_else(|_| {
                        if raw_body.trim().is_empty() {
                            json!({ "error": "Unknown GLM OCR response" })
                        } else {
                            json!({ "raw": format_response_body_for_error(&raw_body) })
                        }
                    });

                final_status = Some(status);
                final_value = parsed_value;
                final_raw_body = raw_body;

                if should_retry_http_status(status) && attempt + 1 < MODEL_REQUEST_MAX_ATTEMPTS {
                    sleep_with_backoff(attempt).await;
                    continue;
                }
                break;
            }

            let Some(status) = final_status else {
                continue;
            };
            let value = final_value;

            if !status.is_success() {
                let detail = if final_raw_body.trim().is_empty() {
                    value.to_string()
                } else {
                    format_response_body_for_error(&final_raw_body)
                };
                last_error = Some(format!("GLM OCR 调用失败（{}）: {}", status, detail));
                let is_auth_error = status.as_u16() == 401 || status.as_u16() == 403;
                if auth_index == 0 && !is_auth_error {
                    break;
                }
                continue;
            }

            let text = extract_vision_ocr_text(&value);
            if text.trim().is_empty() {
                last_error = Some(format!("GLM OCR 返回内容为空: {}", value));
                continue;
            }
            return Ok(text.trim().to_string());
        }
    }

    Err(CommandError::Settings(last_error.unwrap_or_else(|| {
        "GLM OCR 调用失败，请检查接口地址、API Key 和模型配置".to_string()
    })))
}

fn extract_stream_delta_text(value: &serde_json::Value) -> String {
    let Some(choice) = value.get("choices").and_then(|choices| choices.get(0)) else {
        return String::new();
    };

    if let Some(content) = choice
        .get("delta")
        .and_then(|delta| delta.get("content"))
        .and_then(|content| content.as_str())
    {
        return content.to_string();
    }

    if let Some(items) = choice
        .get("delta")
        .and_then(|delta| delta.get("content"))
        .and_then(|content| content.as_array())
    {
        let mut merged = String::new();
        for item in items {
            if let Some(text) = item.get("text").and_then(|value| value.as_str()) {
                merged.push_str(text);
            }
        }
        return merged;
    }

    String::new()
}

fn is_stream_payload_finished(value: &serde_json::Value) -> bool {
    value
        .get("choices")
        .and_then(|choices| choices.get(0))
        .and_then(|choice| choice.get("finish_reason"))
        .and_then(|reason| reason.as_str())
        .map(|reason| !reason.is_empty())
        .unwrap_or(false)
}

fn process_stream_data_payload(
    payload: &str,
    content: &mut String,
    on_delta: &mut impl FnMut(&str) -> bool,
) -> Result<bool, CommandError> {
    if payload.is_empty() {
        return Ok(false);
    }
    if payload == "[DONE]" {
        return Ok(true);
    }

    let Ok(value) = serde_json::from_str::<serde_json::Value>(payload) else {
        return Ok(false);
    };

    if let Some(error) = value.get("error") {
        return Err(CommandError::Settings(format!("模型流式响应错误: {error}")));
    }

    let delta = extract_stream_delta_text(&value);
    if !delta.is_empty() {
        content.push_str(&delta);
        if !on_delta(&delta) {
            return Err(CommandError::Settings(TASK_REPLACED_ERROR.to_string()));
        }
    }

    Ok(is_stream_payload_finished(&value))
}

fn parse_sse_text_content(
    body: &str,
    on_delta: &mut impl FnMut(&str) -> bool,
) -> Result<(String, bool), CommandError> {
    let mut content = String::new();
    let mut event_data = String::new();
    let mut saw_data = false;

    let mut flush_event = |event_data: &mut String,
                           content: &mut String,
                           saw_data: &mut bool|
     -> Result<bool, CommandError> {
        if event_data.trim().is_empty() {
            event_data.clear();
            return Ok(false);
        }
        *saw_data = true;
        let payload = event_data.trim().to_string();
        event_data.clear();
        process_stream_data_payload(&payload, content, on_delta)
    };

    for raw_line in body.lines() {
        let line = raw_line.trim_end_matches('\r');
        if line.trim().is_empty() {
            if flush_event(&mut event_data, &mut content, &mut saw_data)? {
                return Ok((content, true));
            }
            continue;
        }
        if let Some(payload) = line.strip_prefix("data:") {
            if !event_data.is_empty() {
                event_data.push('\n');
            }
            event_data.push_str(payload.trim_start());
        }
    }

    let done = flush_event(&mut event_data, &mut content, &mut saw_data)?;
    Ok((content, saw_data || done))
}

async fn read_response_body_lossy(response: reqwest::Response) -> Result<String, reqwest::Error> {
    let mut response = response;
    let mut bytes = Vec::<u8>::new();
    loop {
        match response.chunk().await {
            Ok(Some(chunk)) => bytes.extend_from_slice(&chunk),
            Ok(None) => break,
            Err(error) => {
                if bytes.is_empty() {
                    return Err(error);
                }
                eprintln!(
                    "[HTTP] body read interrupted after {} bytes, using partial content: {}",
                    bytes.len(),
                    error
                );
                break;
            }
        }
    }
    Ok(String::from_utf8_lossy(&bytes).to_string())
}

async fn call_llm_for_action(
    client: &reqwest::Client,
    llm: &LlmSettings,
    system_prompt: &str,
    user_prompt: &str,
    stream_enabled: bool,
    mut on_delta: impl FnMut(&str) -> bool,
) -> Result<String, CommandError> {
    if !llm.enabled || llm.api_key.trim().is_empty() {
        return Err(CommandError::Settings(
            "请先在设置中配置并启用大模型 API".to_string(),
        ));
    }

    let request_body = json!({
        "model": llm.model,
        "temperature": llm.temperature,
        "max_tokens": llm.max_tokens,
        "stream": stream_enabled,
        "messages": [
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": user_prompt}
        ]
    });
    let mut last_error: Option<String> = None;

    for attempt in 0..MODEL_REQUEST_MAX_ATTEMPTS {
        let mut response = match client
            .post(llm.base_url.trim())
            .timeout(Duration::from_millis(llm.timeout_ms))
            .header(AUTHORIZATION, format!("Bearer {}", llm.api_key.trim()))
            .header(CONTENT_TYPE, "application/json")
            .json(&request_body)
            .send()
            .await
        {
            Ok(resp) => resp,
            Err(error) => {
                if should_retry_network_error(&error) && attempt + 1 < MODEL_REQUEST_MAX_ATTEMPTS {
                    last_error = Some(error.to_string());
                    sleep_with_backoff(attempt).await;
                    continue;
                }
                return Err(CommandError::Settings(error.to_string()));
            }
        };

        let status = response.status();
        if !status.is_success() {
            let raw_body = read_response_body_lossy(response)
                .await
                .unwrap_or_else(|error| format!("(failed to read response body: {error})"));
            let message = format!(
                "模型接口调用失败（{}）: {}",
                status,
                format_response_body_for_error(&raw_body)
            );
            if should_retry_http_status(status) && attempt + 1 < MODEL_REQUEST_MAX_ATTEMPTS {
                last_error = Some(message);
                sleep_with_backoff(attempt).await;
                continue;
            }
            return Err(CommandError::Settings(message));
        }

        let content_type = response
            .headers()
            .get(CONTENT_TYPE)
            .and_then(|value| value.to_str().ok())
            .unwrap_or("")
            .to_ascii_lowercase();
        let mut content = String::new();

        if content_type.contains("text/event-stream") {
            let mut line_buf = String::new();
            let mut event_data = String::new();
            let mut stream_raw = String::new();
            let mut stream_done = false;
            let mut should_retry_stream = false;

            let mut flush_event =
                |event_data: &mut String, content: &mut String| -> Result<bool, CommandError> {
                    if event_data.trim().is_empty() {
                        event_data.clear();
                        return Ok(false);
                    }
                    let payload = event_data.trim().to_string();
                    event_data.clear();
                    process_stream_data_payload(&payload, content, &mut on_delta)
                };

            loop {
                let chunk = match response.chunk().await {
                    Ok(value) => value,
                    Err(error) => {
                        if content.is_empty()
                            && should_retry_network_error(&error)
                            && attempt + 1 < MODEL_REQUEST_MAX_ATTEMPTS
                        {
                            should_retry_stream = true;
                            last_error = Some(error.to_string());
                            break;
                        }
                        if !content.is_empty() && should_retry_network_error(&error) {
                            // Some providers may close or truncate chunked responses after
                            // streaming useful content. Keep partial output instead of failing.
                            eprintln!(
                                "[LLM] stream body read interrupted, using partial output: {}",
                                error
                            );
                            break;
                        }
                        return Err(CommandError::Settings(error.to_string()));
                    }
                };

                let Some(chunk) = chunk else {
                    break;
                };

                let chunk_text = String::from_utf8_lossy(&chunk);
                stream_raw.push_str(&chunk_text);
                line_buf.push_str(&chunk_text);

                while let Some(line_end) = line_buf.find('\n') {
                    let raw_line = line_buf[..line_end].to_string();
                    line_buf.drain(..=line_end);
                    let line = raw_line.trim_end_matches('\r');

                    if line.trim().is_empty() {
                        if flush_event(&mut event_data, &mut content)? {
                            stream_done = true;
                            break;
                        }
                        continue;
                    }

                    if let Some(payload) = line.strip_prefix("data:") {
                        if !event_data.is_empty() {
                            event_data.push('\n');
                        }
                        event_data.push_str(payload.trim_start());
                    }
                }

                if stream_done {
                    break;
                }
            }

            if should_retry_stream {
                sleep_with_backoff(attempt).await;
                continue;
            }

            if !stream_done {
                let tail = line_buf.trim_end_matches('\r').trim();
                if let Some(payload) = tail.strip_prefix("data:") {
                    if !event_data.is_empty() {
                        event_data.push('\n');
                    }
                    event_data.push_str(payload.trim_start());
                }
                if !event_data.trim().is_empty() {
                    let _ = flush_event(&mut event_data, &mut content)?;
                }
            }

            if content.trim().is_empty() && !stream_raw.trim().is_empty() {
                let (fallback_content, saw_sse) =
                    parse_sse_text_content(&stream_raw, &mut on_delta)?;
                if saw_sse {
                    content = fallback_content;
                } else if let Ok(value) = serde_json::from_str::<serde_json::Value>(&stream_raw) {
                    content = extract_llm_text_content(&value);
                }
            }
        } else {
            let body_text = match read_response_body_lossy(response).await {
                Ok(value) => value,
                Err(error) => {
                    if should_retry_network_error(&error)
                        && attempt + 1 < MODEL_REQUEST_MAX_ATTEMPTS
                    {
                        last_error = Some(error.to_string());
                        sleep_with_backoff(attempt).await;
                        continue;
                    }
                    return Err(CommandError::Settings(error.to_string()));
                }
            };
            if let Ok(value) = serde_json::from_str::<serde_json::Value>(&body_text) {
                content = extract_llm_text_content(&value);
            }
            if content.trim().is_empty() {
                let (fallback_content, saw_sse) =
                    parse_sse_text_content(&body_text, &mut on_delta)?;
                if saw_sse {
                    content = fallback_content;
                } else {
                    content = body_text.trim().to_string();
                }
            }
        }

        let normalized = content.trim().to_string();
        if !normalized.is_empty() {
            return Ok(normalized);
        }

        let message = "模型返回内容为空，请检查模型和提示词配置".to_string();
        if attempt + 1 < MODEL_REQUEST_MAX_ATTEMPTS {
            last_error = Some(message);
            sleep_with_backoff(attempt).await;
            continue;
        }
        return Err(CommandError::Settings(message));
    }

    Err(CommandError::Settings(last_error.unwrap_or_else(|| {
        "模型接口调用失败，请稍后重试".to_string()
    })))
}

fn rgba_image_to_data_url(image: &RgbaImage) -> Result<String, CommandError> {
    let dynamic = DynamicImage::ImageRgba8(image.clone());
    let mut cursor = Cursor::new(Vec::<u8>::new());
    dynamic
        .write_to(&mut cursor, ImageFormat::Png)
        .map_err(|error| CommandError::InvalidImage(error.to_string()))?;
    let encoded = BASE64.encode(cursor.into_inner());
    Ok(format!("data:image/png;base64,{encoded}"))
}

fn refresh_ocr_capture_snapshot(ocr_runtime: &OcrRuntimeState) -> Result<(), CommandError> {
    let pointer = current_pointer_position().unwrap_or(PhysicalPosition::new(40, 40));
    let screens = Screen::all().map_err(|error| CommandError::Settings(error.to_string()))?;
    let screen = screens
        .into_iter()
        .find(|screen| {
            let info = screen.display_info;
            pointer.x >= info.x
                && pointer.x < info.x.saturating_add(info.width as i32)
                && pointer.y >= info.y
                && pointer.y < info.y.saturating_add(info.height as i32)
        })
        .ok_or_else(|| CommandError::Settings("无法定位 OCR 截图显示器".to_string()))?;

    let info = screen.display_info;
    let image = screen
        .capture()
        .map_err(|error| CommandError::Settings(error.to_string()))?;

    let mut guard = ocr_runtime
        .capture_snapshot
        .lock()
        .map_err(|_| CommandError::Lock)?;
    *guard = Some(OcrCaptureSnapshot {
        monitor_x: info.x,
        monitor_y: info.y,
        image,
    });
    Ok(())
}

fn capture_ocr_area_to_data_url<R: Runtime>(
    app: &AppHandle<R>,
    ocr_runtime: &OcrRuntimeState,
    area: &OcrCaptureAreaPayload,
) -> Result<String, CommandError> {
    let Some(window) = app.get_webview_window(OCR_CAPTURE_WINDOW_LABEL) else {
        return Err(CommandError::Settings(
            "OCR capture window not found".to_string(),
        ));
    };

    if area.width < 2.0 || area.height < 2.0 {
        return Err(CommandError::Settings("请选择有效的截图区域".to_string()));
    }

    let window_pos = window
        .outer_position()
        .map_err(|error| CommandError::Settings(error.to_string()))?;
    let scale_factor = window.scale_factor().unwrap_or(1.0);

    let left = window_pos.x + (area.x * scale_factor).round() as i32;
    let top = window_pos.y + (area.y * scale_factor).round() as i32;
    let mut width = (area.width * scale_factor).round().max(1.0) as u32;
    let mut height = (area.height * scale_factor).round().max(1.0) as u32;

    if let Ok(snapshot_guard) = ocr_runtime.capture_snapshot.lock() {
        if let Some(snapshot) = snapshot_guard.as_ref() {
            let capture_x = left.saturating_sub(snapshot.monitor_x);
            let capture_y = top.saturating_sub(snapshot.monitor_y);
            let available_w = (snapshot.image.width() as i32)
                .saturating_sub(capture_x)
                .max(1) as u32;
            let available_h = (snapshot.image.height() as i32)
                .saturating_sub(capture_y)
                .max(1) as u32;
            let cropped_w = width.min(available_w);
            let cropped_h = height.min(available_h);

            if capture_x >= 0
                && capture_y >= 0
                && (capture_x as u32) < snapshot.image.width()
                && (capture_y as u32) < snapshot.image.height()
            {
                let cropped = image::imageops::crop_imm(
                    &snapshot.image,
                    capture_x as u32,
                    capture_y as u32,
                    cropped_w,
                    cropped_h,
                )
                .to_image();
                return rgba_image_to_data_url(&cropped);
            }
        }
    }

    let screens = Screen::all().map_err(|error| CommandError::Settings(error.to_string()))?;
    let screen = screens
        .into_iter()
        .find(|screen| {
            let info = screen.display_info;
            left >= info.x
                && left < info.x.saturating_add(info.width as i32)
                && top >= info.y
                && top < info.y.saturating_add(info.height as i32)
        })
        .ok_or_else(|| CommandError::Settings("无法定位截图区域所在显示器".to_string()))?;

    let info = screen.display_info;
    let capture_x = left.saturating_sub(info.x);
    let capture_y = top.saturating_sub(info.y);
    let available_w = (info.width as i32).saturating_sub(capture_x).max(1) as u32;
    let available_h = (info.height as i32).saturating_sub(capture_y).max(1) as u32;
    width = width.min(available_w);
    height = height.min(available_h);

    let image = screen
        .capture_area(capture_x, capture_y, width, height)
        .map_err(|error| CommandError::Settings(error.to_string()))?;
    rgba_image_to_data_url(&image)
}

async fn call_vision_ocr(
    client: &reqwest::Client,
    vision: &VisionSettings,
    image_data_url: &str,
) -> Result<String, CommandError> {
    if vision.api_key.trim().is_empty() {
        return Err(CommandError::Settings(
            "请先在智能 OCR 设置中填写视觉模型 API Key".to_string(),
        ));
    }

    if should_use_glm_layout_parsing(vision) {
        return call_glm_layout_parsing_ocr(client, vision, image_data_url).await;
    }

    let request_body = json!({
        "model": vision.model,
        "temperature": vision.temperature,
        "max_tokens": vision.max_tokens,
        "stream": false,
        "messages": [
            {
                "role": "system",
                "content": "You are an OCR extraction engine.\nRequirements:\n- Extract only text that is visibly present in the image.\n- Preserve the original language; do not translate.\n- Keep reading order and line breaks when possible.\n- Do not infer or fabricate missing content.\n- Output plain text only."
            },
            {
                "role": "user",
                "content": [
                    {"type":"text", "text":"Extract all legible text from this image. Keep punctuation and line breaks where useful. If no readable text exists, return an empty string."},
                    {"type":"image_url", "image_url": {"url": image_data_url}}
                ]
            }
        ]
    });
    let mut last_error: Option<String> = None;
    for attempt in 0..MODEL_REQUEST_MAX_ATTEMPTS {
        let response = match client
            .post(vision.base_url.trim())
            .timeout(Duration::from_millis(vision.timeout_ms))
            .header(
                AUTHORIZATION,
                build_auth_header_value(&vision.api_key, true),
            )
            .header(CONTENT_TYPE, "application/json")
            .json(&request_body)
            .send()
            .await
        {
            Ok(resp) => resp,
            Err(error) => {
                if should_retry_network_error(&error) && attempt + 1 < MODEL_REQUEST_MAX_ATTEMPTS {
                    last_error = Some(error.to_string());
                    sleep_with_backoff(attempt).await;
                    continue;
                }
                return Err(CommandError::Settings(error.to_string()));
            }
        };

        let status = response.status();
        let raw_body = match read_response_body_lossy(response).await {
            Ok(value) => value,
            Err(error) => {
                if should_retry_network_error(&error) && attempt + 1 < MODEL_REQUEST_MAX_ATTEMPTS {
                    last_error = Some(error.to_string());
                    sleep_with_backoff(attempt).await;
                    continue;
                }
                return Err(CommandError::Settings(error.to_string()));
            }
        };
        if !status.is_success() {
            let message = format!(
                "OCR 视觉模型调用失败（{}）: {}",
                status,
                format_response_body_for_error(&raw_body)
            );
            if should_retry_http_status(status) && attempt + 1 < MODEL_REQUEST_MAX_ATTEMPTS {
                last_error = Some(message);
                sleep_with_backoff(attempt).await;
                continue;
            }
            return Err(CommandError::Settings(message));
        }

        let value = match serde_json::from_str::<serde_json::Value>(&raw_body) {
            Ok(parsed) => parsed,
            Err(error) => {
                if attempt + 1 < MODEL_REQUEST_MAX_ATTEMPTS {
                    last_error = Some(error.to_string());
                    sleep_with_backoff(attempt).await;
                    continue;
                }
                return Err(CommandError::Serialization(error.to_string()));
            }
        };
        let text = extract_llm_text_content(&value);
        if !text.trim().is_empty() {
            return Ok(text.trim().to_string());
        }

        let message = "OCR 识别结果为空，请调整截图区域或模型配置".to_string();
        if attempt + 1 < MODEL_REQUEST_MAX_ATTEMPTS {
            last_error = Some(message);
            sleep_with_backoff(attempt).await;
            continue;
        }
        return Err(CommandError::Settings(message));
    }

    Err(CommandError::Settings(last_error.unwrap_or_else(|| {
        "OCR 视觉模型调用失败，请稍后重试".to_string()
    })))
}

async fn test_openai_compatible_model(
    client: &reqwest::Client,
    base_url: &str,
    api_key: &str,
    model: &str,
    timeout_ms: u64,
) -> Result<String, CommandError> {
    let request_body = json!({
        "model": model,
        "temperature": 0,
        "max_tokens": 32,
        "stream": false,
        "messages": [
            {"role": "system", "content": "Reply with OK only."},
            {"role": "user", "content": "API health check"}
        ]
    });

    let response = client
        .post(base_url.trim())
        .timeout(Duration::from_millis(timeout_ms))
        .header(AUTHORIZATION, format!("Bearer {}", api_key.trim()))
        .header(CONTENT_TYPE, "application/json")
        .json(&request_body)
        .send()
        .await
        .map_err(|error| CommandError::Settings(error.to_string()))?;

    let status = response.status();
    let raw_body = read_response_body_lossy(response)
        .await
        .map_err(|error| CommandError::Settings(error.to_string()))?;
    if !status.is_success() {
        return Err(CommandError::Settings(format!(
            "API 调用失败（{}）: {}",
            status,
            format_response_body_for_error(&raw_body)
        )));
    }

    if let Ok(value) = serde_json::from_str::<serde_json::Value>(&raw_body) {
        let content = extract_llm_text_content(&value).trim().to_string();
        if !content.is_empty() {
            return Ok(content);
        }
    }

    let fallback = raw_body.trim();
    if !fallback.is_empty() {
        return Ok(fallback.chars().take(80).collect());
    }

    Ok("OK".to_string())
}

fn show_window_by_label<R: Runtime>(app: &AppHandle<R>, label: &str, focus: bool) {
    if let Some(window) = app.get_webview_window(label) {
        let _ = window.show();
        let _ = window.unminimize();
        if focus {
            let _ = window.set_focus();
        }
    }
}

fn hide_window_by_label<R: Runtime>(app: &AppHandle<R>, label: &str) {
    if let Some(window) = app.get_webview_window(label) {
        if label == MAIN_WINDOW_LABEL {
            if let Some(settings_state) = app.try_state::<AppSettingsState>() {
                let _ = persist_settings_state(&settings_state);
            }
        }
        let _ = window.hide();
    }
}

fn apply_saved_main_window_size<R: Runtime>(app: &AppHandle<R>) {
    let Some(settings_state) = app.try_state::<AppSettingsState>() else {
        return;
    };

    let (remember_size, width, height) = match settings_state.data.lock() {
        Ok(settings) => (
            settings.window.remember_main_window_size,
            settings.main_window_width,
            settings.main_window_height,
        ),
        Err(_) => return,
    };

    if !remember_size {
        return;
    }

    let (Some(width), Some(height)) = (width, height) else {
        return;
    };

    if let Some(window) = app.get_webview_window(MAIN_WINDOW_LABEL) {
        let mut target_width = width;
        let mut target_height = height;

        let should_reset_to_default = window
            .current_monitor()
            .ok()
            .flatten()
            .map(|monitor| {
                let size = monitor.size();
                is_saved_size_near_monitor_bounds(width, height, size.width, size.height)
            })
            .unwrap_or(false);

        if should_reset_to_default {
            target_width = DEFAULT_MAIN_WINDOW_WIDTH;
            target_height = DEFAULT_MAIN_WINDOW_HEIGHT;
        }

        let _ = window.set_size(Size::Logical(LogicalSize::new(
            target_width as f64,
            target_height as f64,
        )));

        if should_reset_to_default {
            if let Ok(mut settings) = settings_state.data.lock() {
                settings.main_window_width = Some(target_width);
                settings.main_window_height = Some(target_height);
            }
            let _ = persist_settings_state(&settings_state);
        }
    }
}

fn show_main_window<R: Runtime>(app: &AppHandle<R>) {
    let settings_snapshot = app
        .try_state::<AppSettingsState>()
        .and_then(|state| state.data.lock().ok().map(|settings| settings.clone()))
        .unwrap_or_else(AppSettings::default);
    let _ = app.emit(
        MAIN_WINDOW_SHOWN_EVENT,
        MainWindowShownPayload {
            collapse_top_bar: settings_snapshot.history.collapse_top_bar,
            open_to_top: settings_snapshot.history.open_at_top_on_show,
        },
    );

    let main_focused = app
        .get_webview_window(MAIN_WINDOW_LABEL)
        .and_then(|window| window.is_focused().ok())
        .unwrap_or(false);
    let settings_focused = app
        .get_webview_window(SETTINGS_WINDOW_LABEL)
        .and_then(|window| window.is_focused().ok())
        .unwrap_or(false);
    let runtime_flags = app.state::<RuntimeFlags>();
    if !main_focused && !settings_focused {
        capture_last_foreground_window(&runtime_flags);
    }
    apply_saved_main_window_size(app);
    apply_main_window_position(app);
    show_window_by_label(app, MAIN_WINDOW_LABEL, true);
}

fn show_settings_window<R: Runtime>(app: &AppHandle<R>) {
    show_window_by_label(app, SETTINGS_WINDOW_LABEL, true);
    let _ = app.emit(SETTINGS_WINDOW_SHOWN_EVENT, true);
}

fn toggle_main_window<R: Runtime>(app: &AppHandle<R>) {
    if let Some(window) = app.get_webview_window(MAIN_WINDOW_LABEL) {
        let visible = window.is_visible().unwrap_or(false);

        if visible {
            if let Some(settings_state) = app.try_state::<AppSettingsState>() {
                let _ = persist_settings_state(&settings_state);
            }
            let _ = window.hide();
        } else {
            show_main_window(app);
        }
    }
}

#[derive(Clone, Copy)]
enum ShortcutAction {
    ToggleMain,
    StartOcrCapture,
}

fn toggle_ocr_capture_window<R: Runtime>(app: &AppHandle<R>) {
    let capture_visible = app
        .get_webview_window(OCR_CAPTURE_WINDOW_LABEL)
        .and_then(|window| window.is_visible().ok())
        .unwrap_or(false);

    if capture_visible {
        if let Some(ocr_runtime) = app.try_state::<OcrRuntimeState>() {
            ocr_runtime.capture_active.store(false, Ordering::Relaxed);
            ocr_runtime
                .suppress_blur_until_ms
                .store(0, Ordering::Relaxed);
            if let Ok(mut snapshot) = ocr_runtime.capture_snapshot.lock() {
                *snapshot = None;
            }
        }
        hide_ocr_capture_window(app);
        emit_ocr_capture_canceled(app);
        return;
    }

    if let Err(error) = start_ocr_capture_workflow(app) {
        emit_ocr_error(app, &error.to_string());
    }
}

fn register_or_replace_shortcut<R: Runtime>(
    app: &AppHandle<R>,
    previous: Option<&str>,
    next: &str,
    action: ShortcutAction,
) -> Result<(), CommandError> {
    if let Some(old) = previous {
        if old == next {
            return Ok(());
        }

        if app.global_shortcut().is_registered(old) {
            app.global_shortcut()
                .unregister(old)
                .map_err(|error| CommandError::Shortcut(error.to_string()))?;
        }
    }

    app.global_shortcut()
        .on_shortcut(next, move |app, _shortcut, event| {
            if event.state == ShortcutState::Pressed {
                match action {
                    ShortcutAction::ToggleMain => toggle_main_window(app),
                    ShortcutAction::StartOcrCapture => toggle_ocr_capture_window(app),
                }
            }
        })
        .map_err(|error| CommandError::Shortcut(error.to_string()))
}

fn apply_shortcut_change<R: Runtime>(
    app: &AppHandle<R>,
    previous: &str,
    next: &str,
    action: ShortcutAction,
) -> Result<(), CommandError> {
    if previous == next {
        return Ok(());
    }

    register_or_replace_shortcut(app, Some(previous), next, action).inspect_err(|_error| {
        let _ = register_or_replace_shortcut(app, Some(next), previous, action);
    })
}

fn create_tray<R: Runtime>(app: &AppHandle<R>) -> tauri::Result<()> {
    let language = app
        .try_state::<AppSettingsState>()
        .and_then(|state| {
            state
                .data
                .lock()
                .ok()
                .map(|settings| settings.language.clone())
        })
        .unwrap_or_else(|| "zh-CN".to_string());
    let (open_main_label, open_ocr_label, open_settings_label, quit_label, tooltip) =
        if language.eq_ignore_ascii_case("en-US") {
            (
                "Open SnapParse",
                "OCR Capture",
                "Open Settings",
                "Quit",
                "SnapParse",
            )
        } else {
            (
                "打开 SnapParse",
                "OCR 识别",
                "打开设置",
                "退出",
                "SnapParse",
            )
        };

    let menu = MenuBuilder::new(app)
        .text(TRAY_MENU_MAIN_ID, open_main_label)
        .text(TRAY_MENU_OCR_ID, open_ocr_label)
        .text(TRAY_MENU_SETTINGS_ID, open_settings_label)
        .separator()
        .text(TRAY_MENU_QUIT_ID, quit_label)
        .build()?;

    let mut tray_builder = TrayIconBuilder::with_id(TRAY_ID)
        .menu(&menu)
        .show_menu_on_left_click(false)
        .tooltip(tooltip);

    if let Some(icon) = app.default_window_icon().cloned() {
        tray_builder = tray_builder.icon(icon);
    }

    let _tray = tray_builder
        .on_menu_event(|app, event| match event.id().as_ref() {
            TRAY_MENU_MAIN_ID => show_main_window(app),
            TRAY_MENU_OCR_ID => toggle_ocr_capture_window(app),
            TRAY_MENU_SETTINGS_ID => show_settings_window(app),
            TRAY_MENU_QUIT_ID => {
                if let Some(settings_state) = app.try_state::<AppSettingsState>() {
                    let _ = persist_settings_state(&settings_state);
                }
                let flags = app.state::<RuntimeFlags>();
                flags.allow_exit.store(true, Ordering::Relaxed);
                app.exit(0);
            }
            _ => {}
        })
        .on_tray_icon_event(|tray, event| {
            if let TrayIconEvent::Click {
                button: MouseButton::Left,
                button_state: MouseButtonState::Up,
                ..
            } = event
            {
                toggle_main_window(tray.app_handle());
            }
        })
        .build(app)?;

    Ok(())
}

fn rebuild_tray<R: Runtime>(app: &AppHandle<R>) {
    let _ = app.remove_tray_by_id(TRAY_ID);
    if let Err(error) = create_tray(app) {
        eprintln!("Failed to rebuild tray icon: {error}");
    }
}
fn to_image_data(data_url: &str) -> Result<ImageData<'static>, CommandError> {
    let (_, payload) = data_url
        .split_once(',')
        .ok_or_else(|| CommandError::InvalidImage("Invalid image data URL".to_string()))?;

    let bytes = BASE64
        .decode(payload)
        .map_err(|error| CommandError::InvalidImage(error.to_string()))?;

    let rgba = image::load_from_memory(&bytes)
        .map_err(|error| CommandError::InvalidImage(error.to_string()))?
        .to_rgba8();
    let (width, height) = rgba.dimensions();

    Ok(ImageData {
        width: width as usize,
        height: height as usize,
        bytes: Cow::Owned(rgba.into_raw()),
    })
}

fn trim_history_by_settings(
    history_state: &State<'_, Mutex<ClipboardState>>,
    settings: &AppSettings,
) -> Result<(), CommandError> {
    let mut history = with_history_lock(history_state)?;
    trim_history(&mut history.history, settings.history.max_items);
    normalize_history_order(&mut history.history);
    Ok(())
}

fn sync_autostart_with_settings<R: Runtime>(
    app: &AppHandle<R>,
    settings: &AppSettings,
) -> Result<(), CommandError> {
    let running_from_debug_target = std::env::current_exe()
        .ok()
        .and_then(|path| path.to_str().map(|value| value.to_ascii_lowercase()))
        .map(|path| path.contains("\\target\\debug\\") || path.contains("/target/debug/"))
        .unwrap_or(false);
    if running_from_debug_target {
        // Prevent development runs from overwriting production auto-start entries.
        return Ok(());
    }

    let Some(manager) = app.try_state::<AutoLaunchManager>() else {
        return Ok(());
    };
    let enabled = manager.is_enabled().map_err(|error| {
        CommandError::Settings(format!("Auto-start state check failed: {error}"))
    })?;

    if settings.window.launch_on_system_startup {
        // Always refresh registration so stale entries (for example debug executable paths)
        // are replaced with the current installed executable path.
        if enabled {
            manager.disable().map_err(|error| {
                CommandError::Settings(format!("Disable auto-start failed: {error}"))
            })?;
        }
        manager.enable().map_err(|error| {
            CommandError::Settings(format!("Enable auto-start failed: {error}"))
        })?;
    } else if enabled {
        manager.disable().map_err(|error| {
            CommandError::Settings(format!("Disable auto-start failed: {error}"))
        })?;
    }

    Ok(())
}

fn schedule_autostart_sync_retry<R: Runtime>(app: AppHandle<R>) {
    std::thread::spawn(move || {
        for _ in 0..30 {
            if app.try_state::<AutoLaunchManager>().is_none() {
                std::thread::sleep(Duration::from_millis(250));
                continue;
            }

            let settings_snapshot = app
                .try_state::<AppSettingsState>()
                .and_then(|state| state.data.lock().ok().map(|settings| settings.clone()));

            if let Some(settings) = settings_snapshot {
                if let Err(error) = sync_autostart_with_settings(&app, &settings) {
                    eprintln!("[AutoStart] delayed sync failed: {error}");
                }
            }
            return;
        }
        eprintln!("[AutoStart] delayed sync skipped: manager state unavailable");
    });
}

fn parse_tasklist_executable_name(line: &str) -> Option<String> {
    let trimmed = line.trim();
    if trimmed.is_empty() {
        return None;
    }

    let name = if let Some(rest) = trimmed.strip_prefix('"') {
        let end_index = rest.find('"')?;
        &rest[..end_index]
    } else {
        trimmed.split(',').next().unwrap_or_default().trim()
    };

    let value = name.trim();
    if value.is_empty() {
        return None;
    }
    if !value.to_ascii_lowercase().ends_with(".exe") {
        return None;
    }
    Some(value.to_string())
}

#[tauri::command]
fn list_running_apps_cmd() -> Result<Vec<String>, CommandError> {
    #[cfg(target_os = "windows")]
    {
        let mut command = Command::new("tasklist");
        configure_background_command(&mut command);
        let output = command
            .args(["/FO", "CSV", "/NH"])
            .stdin(Stdio::null())
            .stdout(Stdio::piped())
            .stderr(Stdio::null())
            .output()
            .map_err(|error| {
                CommandError::Settings(format!("获取运行中应用失败（tasklist）: {error}"))
            })?;

        if !output.status.success() {
            return Err(CommandError::Settings(format!(
                "获取运行中应用失败，tasklist 退出码: {}",
                output.status
            )));
        }

        let mut unique = HashSet::<String>::new();
        let mut items = Vec::<String>::new();
        let text = String::from_utf8_lossy(&output.stdout);
        for line in text.lines() {
            let Some(name) = parse_tasklist_executable_name(line) else {
                continue;
            };
            let key = name.to_ascii_lowercase();
            if unique.insert(key) {
                items.push(name);
            }
        }
        items.sort_by_key(|a| a.to_ascii_lowercase());
        items.truncate(600);
        Ok(items)
    }

    #[cfg(not(target_os = "windows"))]
    {
        Ok(Vec::new())
    }
}

fn update_settings_internal(
    app: &AppHandle,
    settings_state: &State<'_, AppSettingsState>,
    history_state: &State<'_, Mutex<ClipboardState>>,
    patch: SettingsPatch,
) -> Result<AppSettings, CommandError> {
    let (previous_settings, updated_settings) = {
        let mut settings = with_settings_lock(settings_state)?;
        let previous = settings.clone();
        apply_settings_patch(&mut settings, patch);
        (previous, settings.clone())
    };

    if let Err(error) = apply_shortcut_change(
        app,
        &previous_settings.shortcuts.toggle_main,
        &updated_settings.shortcuts.toggle_main,
        ShortcutAction::ToggleMain,
    ) {
        if let Ok(mut settings) = with_settings_lock(settings_state) {
            *settings = previous_settings;
        }
        return Err(error);
    }
    if let Err(error) = apply_shortcut_change(
        app,
        &previous_settings.shortcuts.toggle_ocr,
        &updated_settings.shortcuts.toggle_ocr,
        ShortcutAction::StartOcrCapture,
    ) {
        let _ = apply_shortcut_change(
            app,
            &updated_settings.shortcuts.toggle_main,
            &previous_settings.shortcuts.toggle_main,
            ShortcutAction::ToggleMain,
        );
        if let Ok(mut settings) = with_settings_lock(settings_state) {
            *settings = previous_settings;
        }
        return Err(error);
    }

    persist_settings(&settings_state.file_path, &updated_settings)?;
    trim_history_by_settings(history_state, &updated_settings)?;
    {
        let history_snapshot = with_history_lock(history_state)?
            .history
            .iter()
            .cloned()
            .collect::<Vec<_>>();
        persist_history_snapshot(app, &updated_settings, &history_snapshot)?;
    }

    if previous_settings.window.launch_on_system_startup
        != updated_settings.window.launch_on_system_startup
    {
        sync_autostart_with_settings(app, &updated_settings)?;
    }

    if let Some(window) = app.get_webview_window(SELECTION_RESULT_WINDOW_LABEL) {
        let _ = window.set_always_on_top(
            updated_settings
                .selection_assistant
                .result_window_always_on_top,
        );
    }
    if let Some(window) = app.get_webview_window(OCR_RESULT_WINDOW_LABEL) {
        let _ = window.set_always_on_top(updated_settings.ocr.result_window_always_on_top);
    }
    apply_result_windows_background(app, &updated_settings);
    if !updated_settings.selection_assistant.enabled {
        hide_selection_bar_window(app);
    }
    if !updated_settings.ocr.enabled {
        hide_ocr_capture_window(app);
    }
    if previous_settings.language != updated_settings.language {
        rebuild_tray(app);
    }

    emit_settings_updated(app, &updated_settings);
    Ok(updated_settings)
}

fn selection_text_in_range(settings: &SelectionAssistantSettings, text: &str) -> bool {
    let trimmed = text.trim();
    let len = trimmed.chars().count();
    len >= settings.min_chars && len <= settings.max_chars
}

fn stable_text_hash(value: &str) -> u64 {
    let mut hasher = std::collections::hash_map::DefaultHasher::new();
    value.hash(&mut hasher);
    hasher.finish()
}

fn remember_latest_selection(
    runtime: &SelectionRuntimeState,
    text: &str,
    source_hwnd: isize,
    mode: SelectionTriggerMode,
    allow_repeat_same: bool,
) -> bool {
    let mut guard = match runtime.last_dispatch_marker.lock() {
        Ok(value) => value,
        Err(_) => return false,
    };
    let normalized_text = text.trim();
    if normalized_text.is_empty() {
        return false;
    }

    let now = now_epoch_millis();
    let text_hash = stable_text_hash(normalized_text);

    if let Some(previous) = *guard {
        let elapsed = now.saturating_sub(previous.emitted_at_ms);
        let same_text = previous.text_hash == text_hash;
        let same_source = previous.source_hwnd == source_hwnd && previous.mode == mode;

        // Fast dedupe window for identical source + payload.
        if same_text && same_source && elapsed < SELECTION_REPEAT_DEDUPE_WINDOW_MS {
            return false;
        }

        // Cooldown for repeated identical payloads when repeat is not requested.
        if same_text && !allow_repeat_same && elapsed < SELECTION_TEXT_COOLDOWN_MS {
            return false;
        }
    }

    *guard = Some(SelectionDispatchMarker {
        text_hash,
        source_hwnd,
        mode,
        emitted_at_ms: now,
    });
    true
}

fn is_task_replaced_error(error: &CommandError) -> bool {
    matches!(error, CommandError::Settings(message) if message == TASK_REPLACED_ERROR)
}

fn begin_selection_result_task<R: Runtime>(app: &AppHandle<R>) -> u64 {
    app.try_state::<SelectionRuntimeState>()
        .map(|runtime| {
            runtime
                .active_result_request_nonce
                .fetch_add(1, Ordering::Relaxed)
                + 1
        })
        .unwrap_or(0)
}

fn is_selection_result_task_active<R: Runtime>(app: &AppHandle<R>, nonce: u64) -> bool {
    if nonce == 0 {
        return true;
    }
    app.try_state::<SelectionRuntimeState>()
        .map(|runtime| runtime.active_result_request_nonce.load(Ordering::Relaxed) == nonce)
        .unwrap_or(true)
}

fn begin_ocr_result_task<R: Runtime>(app: &AppHandle<R>) -> u64 {
    app.try_state::<OcrRuntimeState>()
        .map(|runtime| {
            runtime
                .active_result_request_nonce
                .fetch_add(1, Ordering::Relaxed)
                + 1
        })
        .unwrap_or(0)
}

fn is_ocr_result_task_active<R: Runtime>(app: &AppHandle<R>, nonce: u64) -> bool {
    if nonce == 0 {
        return true;
    }
    app.try_state::<OcrRuntimeState>()
        .map(|runtime| runtime.active_result_request_nonce.load(Ordering::Relaxed) == nonce)
        .unwrap_or(true)
}

fn schedule_selection_bar_auto_hide<R: Runtime>(app: AppHandle<R>, delay_ms: u64) {
    if delay_ms == 0 {
        return;
    }
    std::thread::spawn(move || {
        std::thread::sleep(Duration::from_millis(delay_ms));
        hide_selection_bar_window(&app);
    });
}

fn publish_selection_detected<R: Runtime>(
    app: &AppHandle<R>,
    text: String,
    mode: SelectionTriggerMode,
) -> Result<(), CommandError> {
    if text.trim().is_empty() {
        return Ok(());
    }

    let pointer = current_pointer_position().unwrap_or(PhysicalPosition::new(40, 40));
    let payload = SelectionDetectedPayload {
        text,
        x: pointer.x,
        y: pointer.y,
        mode,
    };
    emit_selection_detected(app, payload.clone());
    show_selection_bar_window(app, &payload)?;

    if let Some(settings_state) = app.try_state::<AppSettingsState>() {
        let delay_ms = settings_state
            .data
            .lock()
            .map(|settings| settings.selection_assistant.auto_hide_ms)
            .unwrap_or(3600);
        schedule_selection_bar_auto_hide(app.clone(), delay_ms);
    }
    Ok(())
}

fn is_any_snapparse_window_focused<R: Runtime>(app: &AppHandle<R>) -> bool {
    [
        MAIN_WINDOW_LABEL,
        SETTINGS_WINDOW_LABEL,
        SELECTION_BAR_WINDOW_LABEL,
        SELECTION_RESULT_WINDOW_LABEL,
        OCR_CAPTURE_WINDOW_LABEL,
        OCR_RESULT_WINDOW_LABEL,
    ]
    .iter()
    .any(|label| {
        app.get_webview_window(label)
            .and_then(|window| window.is_focused().ok())
            .unwrap_or(false)
    })
}

fn start_selection_detector_thread(app: AppHandle) {
    let Some(runtime) = app.try_state::<SelectionRuntimeState>() else {
        return;
    };

    if runtime.detector_running.swap(true, Ordering::Relaxed) {
        return;
    }

    std::thread::spawn(move || {
        let mut last_mouse_down = false;
        let mut last_outside_click_mouse_down = false;
        let mut last_escape_down = false;
        let mut detect_phase = SelectionDetectPhase::Idle;
        let mut mouse_down_started_at_ms: u64 = 0;
        let mut mouse_down_position: Option<PhysicalPosition<i32>> = None;
        let mut mouse_down_started_in_client_area = false;
        let mut drag_foreground_hwnd: isize = 0;
        let mut mouse_down_clipboard_text: Option<String> = None;

        loop {
            std::thread::sleep(Duration::from_millis(120));

            let mouse_down_now = is_left_mouse_pressed();
            let escape_down_now = is_escape_pressed();

            if escape_down_now && !last_escape_down {
                let mut should_cancel_capture = false;
                if let Some(ocr_runtime) = app.try_state::<OcrRuntimeState>() {
                    should_cancel_capture = ocr_runtime.capture_active.load(Ordering::Relaxed);
                    if should_cancel_capture {
                        ocr_runtime.capture_active.store(false, Ordering::Relaxed);
                        ocr_runtime
                            .suppress_blur_until_ms
                            .store(0, Ordering::Relaxed);
                        if let Ok(mut snapshot) = ocr_runtime.capture_snapshot.lock() {
                            *snapshot = None;
                        }
                    }
                }

                if should_cancel_capture {
                    hide_ocr_capture_window(&app);
                    emit_ocr_capture_canceled(&app);
                }
            }
            last_escape_down = escape_down_now;

            let selection_bar_visible = app
                .get_webview_window(SELECTION_BAR_WINDOW_LABEL)
                .and_then(|window| window.is_visible().ok())
                .unwrap_or(false);
            if selection_bar_visible {
                let released = last_outside_click_mouse_down && !mouse_down_now;
                if released {
                    let pointer = current_pointer_position();
                    let inside_bar = pointer
                        .and_then(|point| {
                            app.get_webview_window(SELECTION_BAR_WINDOW_LABEL)
                                .and_then(|window| {
                                    let position = window.outer_position().ok()?;
                                    let size = window.outer_size().ok()?;
                                    let width = to_i32(size.width);
                                    let height = to_i32(size.height);
                                    Some(
                                        point.x >= position.x
                                            && point.x < position.x.saturating_add(width)
                                            && point.y >= position.y
                                            && point.y < position.y.saturating_add(height),
                                    )
                                })
                        })
                        .unwrap_or(false);
                    if !inside_bar {
                        hide_selection_bar_window(&app);
                    }
                }
            }
            last_outside_click_mouse_down = mouse_down_now;

            let Some(settings_state) = app.try_state::<AppSettingsState>() else {
                continue;
            };
            let settings_snapshot = match settings_state.data.lock() {
                Ok(settings) => settings.clone(),
                Err(_) => continue,
            };

            let assistant = settings_snapshot.selection_assistant.clone();
            if !assistant.enabled {
                last_mouse_down = mouse_down_now;
                detect_phase = SelectionDetectPhase::Idle;
                drag_foreground_hwnd = 0;
                mouse_down_started_in_client_area = false;
                mouse_down_clipboard_text = None;
                continue;
            }

            match assistant.mode {
                SelectionTriggerMode::AutoDetect => {
                    if is_any_snapparse_window_focused(&app) {
                        last_mouse_down = mouse_down_now;
                        if !mouse_down_now {
                            detect_phase = SelectionDetectPhase::Idle;
                        }
                        if !mouse_down_now {
                            drag_foreground_hwnd = 0;
                            mouse_down_clipboard_text = None;
                        }
                        continue;
                    }
                    let mouse_down = mouse_down_now;
                    if mouse_down && !last_mouse_down {
                        detect_phase = SelectionDetectPhase::Dragging;
                        mouse_down_started_at_ms = now_epoch_millis();
                        mouse_down_position = current_pointer_position();

                        let flags = app.state::<RuntimeFlags>();
                        capture_last_foreground_window(&flags);
                        drag_foreground_hwnd = flags.last_foreground_hwnd.load(Ordering::Relaxed);
                        mouse_down_started_in_client_area = mouse_down_position
                            .map(|point| {
                                if is_console_like_window(drag_foreground_hwnd) {
                                    !is_point_likely_window_title_bar(drag_foreground_hwnd, point)
                                } else {
                                    true
                                }
                            })
                            .unwrap_or(true);
                        mouse_down_clipboard_text = if is_console_like_window(drag_foreground_hwnd)
                        {
                            read_clipboard_text_trimmed()
                        } else {
                            None
                        };
                    }
                    let just_released = last_mouse_down && !mouse_down;
                    last_mouse_down = mouse_down;
                    if !just_released {
                        continue;
                    }
                    if detect_phase != SelectionDetectPhase::Dragging {
                        continue;
                    }
                    detect_phase = SelectionDetectPhase::Idle;

                    let drag_duration = now_epoch_millis().saturating_sub(mouse_down_started_at_ms);
                    let release_position = current_pointer_position();
                    let drag_distance = match (mouse_down_position, release_position) {
                        (Some(start), Some(end)) => {
                            let dx = (end.x - start.x).unsigned_abs();
                            let dy = (end.y - start.y).unsigned_abs();
                            dx.saturating_add(dy)
                        }
                        _ => 0,
                    };
                    mouse_down_started_at_ms = 0;
                    mouse_down_position = None;

                    let mut hwnd = drag_foreground_hwnd;
                    drag_foreground_hwnd = 0;
                    let started_in_client_area = mouse_down_started_in_client_area;
                    mouse_down_started_in_client_area = false;
                    let clipboard_before_drag = mouse_down_clipboard_text.take();
                    if hwnd == 0 {
                        let flags = app.state::<RuntimeFlags>();
                        capture_last_foreground_window(&flags);
                        hwnd = flags.last_foreground_hwnd.load(Ordering::Relaxed);
                    }
                    if is_window_blocked_by_apps(hwnd, &assistant.blocked_apps) {
                        continue;
                    }

                    if !started_in_client_area {
                        continue;
                    }

                    let is_console = is_console_like_window(hwnd);
                    let (min_duration, min_distance) = if is_console {
                        (120u64, 6u32)
                    } else {
                        (150u64, 10u32)
                    };
                    if drag_duration < min_duration || drag_distance < min_distance {
                        continue;
                    }
                    let Some(runtime_state) = app.try_state::<SelectionRuntimeState>() else {
                        continue;
                    };

                    let (selected, clipboard_changed) = if is_console {
                        match capture_console_selection_without_shortcut(
                            hwnd,
                            clipboard_before_drag.as_deref(),
                        ) {
                            Some(payload) => payload,
                            None => continue,
                        }
                    } else {
                        match capture_selected_text_once(hwnd) {
                            Ok(payload) => payload,
                            Err(_) => continue,
                        }
                    };

                    if !clipboard_changed {
                        continue;
                    }
                    if !selection_text_in_range(&assistant, &selected) {
                        continue;
                    }
                    let allow_repeat_same = if is_console {
                        true
                    } else {
                        app.get_webview_window(SELECTION_BAR_WINDOW_LABEL)
                            .and_then(|window| window.is_visible().ok())
                            .map(|visible| !visible)
                            .unwrap_or(true)
                    };
                    if !remember_latest_selection(
                        &runtime_state,
                        &selected,
                        hwnd,
                        SelectionTriggerMode::AutoDetect,
                        allow_repeat_same,
                    ) {
                        continue;
                    }

                    if let Err(error) =
                        publish_selection_detected(&app, selected, SelectionTriggerMode::AutoDetect)
                    {
                        emit_selection_error(&app, &error.to_string());
                    }
                }
                SelectionTriggerMode::CopyTrigger => {
                    detect_phase = SelectionDetectPhase::Idle;
                    let flags = app.state::<RuntimeFlags>();
                    capture_last_foreground_window(&flags);
                    let active_hwnd = flags.last_foreground_hwnd.load(Ordering::Relaxed);
                    if is_window_blocked_by_apps(active_hwnd, &assistant.blocked_apps) {
                        continue;
                    }
                    let mut clipboard = match Clipboard::new()
                        .map_err(|error| CommandError::Clipboard(error.to_string()))
                    {
                        Ok(value) => value,
                        Err(_) => continue,
                    };
                    let text = clipboard.get_text().unwrap_or_default().trim().to_string();
                    if !selection_text_in_range(&assistant, &text) {
                        continue;
                    }

                    let Some(runtime_state) = app.try_state::<SelectionRuntimeState>() else {
                        continue;
                    };
                    {
                        let mut observed = match runtime_state.last_clipboard_observed.lock() {
                            Ok(value) => value,
                            Err(_) => continue,
                        };
                        if *observed == text {
                            continue;
                        }
                        *observed = text.clone();
                    }
                    if !remember_latest_selection(
                        &runtime_state,
                        &text,
                        active_hwnd,
                        SelectionTriggerMode::CopyTrigger,
                        false,
                    ) {
                        continue;
                    }

                    if let Err(error) =
                        publish_selection_detected(&app, text, SelectionTriggerMode::CopyTrigger)
                    {
                        emit_selection_error(&app, &error.to_string());
                    }
                }
            }
        }
    });
}

#[tauri::command]
fn show_settings_window_cmd(app: AppHandle) {
    show_settings_window(&app);
}

#[tauri::command]
fn open_selection_bar(
    app: AppHandle,
    payload: SelectionBarOpenPayload,
) -> Result<(), CommandError> {
    let normalized_text = payload.text.trim().to_string();
    if normalized_text.is_empty() {
        return Ok(());
    }

    let detected = SelectionDetectedPayload {
        text: normalized_text,
        x: payload.x,
        y: payload.y,
        mode: payload.mode,
    };
    emit_selection_detected(&app, detected.clone());
    show_selection_bar_window(&app, &detected)
}

#[tauri::command]
fn hide_selection_bar(app: AppHandle) {
    hide_selection_bar_window(&app);
}

#[tauri::command]
fn copy_selection_text(text: String, flags: State<'_, RuntimeFlags>) -> Result<(), CommandError> {
    let fallback_value = text;
    let fallback_trimmed = fallback_value.trim().to_string();
    if fallback_trimmed.is_empty() {
        return Ok(());
    }

    #[cfg(target_os = "windows")]
    let hwnd = flags.last_foreground_hwnd.load(Ordering::Relaxed);
    #[cfg(not(target_os = "windows"))]
    let hwnd: isize = 0;

    let value_to_copy =
        try_capture_console_selection_for_copy(hwnd, &fallback_trimmed).unwrap_or(fallback_value);

    write_clipboard_text_with_retry(&value_to_copy)
}

#[tauri::command]
fn open_search_with_text(app: AppHandle, text: String) -> Result<(), CommandError> {
    let value = text.trim();
    if value.is_empty() {
        return Ok(());
    }

    let settings_state = app.state::<AppSettingsState>();
    let template = settings_state
        .data
        .lock()
        .map(|settings| settings.selection_assistant.search_url_template.clone())
        .unwrap_or_else(|_| "https://www.google.com/search?q={query}".to_string());
    let url = build_search_url(&template, value);
    open_in_default_browser(&url)?;
    hide_selection_bar_window(&app);
    Ok(())
}

#[tauri::command]
fn set_result_window_pinned_cmd(app: AppHandle, pinned: bool) -> Result<bool, CommandError> {
    if let Some(window) = app.get_webview_window(SELECTION_RESULT_WINDOW_LABEL) {
        window
            .set_always_on_top(pinned)
            .map_err(|error| CommandError::Settings(error.to_string()))?;
    }

    Ok(pinned)
}

#[tauri::command]
fn get_result_window_pinned_cmd(
    app: AppHandle,
    settings_state: State<'_, AppSettingsState>,
) -> bool {
    if let Some(window) = app.get_webview_window(SELECTION_RESULT_WINDOW_LABEL) {
        if let Ok(pinned) = window.is_always_on_top() {
            return pinned;
        }
    }
    settings_state
        .data
        .lock()
        .map(|settings| settings.selection_assistant.result_window_always_on_top)
        .unwrap_or(true)
}

#[tauri::command]
fn minimize_selection_result_window(app: AppHandle) -> Result<(), CommandError> {
    let Some(window) = app.get_webview_window(SELECTION_RESULT_WINDOW_LABEL) else {
        return Err(CommandError::Settings(
            "Selection result window not found".to_string(),
        ));
    };

    window
        .minimize()
        .map_err(|error| CommandError::Settings(error.to_string()))?;
    Ok(())
}

#[tauri::command]
fn close_selection_result_window(app: AppHandle) -> Result<(), CommandError> {
    let Some(window) = app.get_webview_window(SELECTION_RESULT_WINDOW_LABEL) else {
        return Err(CommandError::Settings(
            "Selection result window not found".to_string(),
        ));
    };

    window
        .hide()
        .map_err(|error| CommandError::Settings(error.to_string()))?;
    Ok(())
}

#[tauri::command]
async fn run_selection_action(
    app: AppHandle,
    payload: RunSelectionActionPayload,
    settings_state: State<'_, AppSettingsState>,
    http_client_state: State<'_, HttpClientState>,
) -> Result<SelectionResultPayload, CommandError> {
    let task_nonce = begin_selection_result_task(&app);
    let source_text = payload.text.trim().to_string();
    if source_text.is_empty() {
        return Err(CommandError::Settings("划词内容为空".to_string()));
    }

    let snapshot = settings_state
        .data
        .lock()
        .map_err(|_| CommandError::Lock)?
        .clone();

    let mut custom_agent_name: Option<String> = None;
    let mut custom_agent_icon: Option<String> = None;
    let mut translate_from: Option<String> = None;
    let mut translate_to: Option<String> = None;

    let (action_name, system_prompt, user_prompt) = match payload.action {
        SelectionActionKind::Summarize => {
            let prompts = choose_builtin_prompt(
                SelectionActionKind::Summarize,
                &source_text,
                None,
                None,
                &snapshot.language,
            );
            ("summary".to_string(), prompts.0, prompts.1)
        }
        SelectionActionKind::Polish => {
            let prompts = choose_builtin_prompt(
                SelectionActionKind::Polish,
                &source_text,
                None,
                None,
                &snapshot.language,
            );
            ("polish".to_string(), prompts.0, prompts.1)
        }
        SelectionActionKind::Explain => {
            let prompts = choose_builtin_prompt(
                SelectionActionKind::Explain,
                &source_text,
                None,
                None,
                &snapshot.language,
            );
            ("explain".to_string(), prompts.0, prompts.1)
        }
        SelectionActionKind::Translate => {
            translate_from = Some(
                payload
                    .translate_from
                    .clone()
                    .unwrap_or_else(|| "auto".to_string()),
            );
            let default_translate_to =
                if snapshot.selection_assistant.default_translate_to.is_empty() {
                    default_translate_target_for_language(&snapshot.language).to_string()
                } else {
                    snapshot.selection_assistant.default_translate_to.clone()
                };
            let requested_translate_to = payload
                .translate_to
                .clone()
                .unwrap_or_else(|| default_translate_to.clone());
            translate_to = Some(normalize_translate_language(
                &requested_translate_to,
                &default_translate_to,
            ));
            let prompts = choose_builtin_prompt(
                SelectionActionKind::Translate,
                &source_text,
                translate_from.as_deref(),
                translate_to.as_deref(),
                &snapshot.language,
            );
            ("translate".to_string(), prompts.0, prompts.1)
        }
        SelectionActionKind::Custom => {
            let custom_id = payload
                .custom_agent_id
                .clone()
                .ok_or_else(|| CommandError::Settings("缺少自定义 Agent ID".to_string()))?;
            let agent = snapshot
                .agents
                .custom
                .iter()
                .find(|item| item.id == custom_id)
                .cloned()
                .ok_or_else(|| CommandError::Settings("未找到可用的自定义 Agent".to_string()))?;
            custom_agent_name = Some(agent.name.clone());
            custom_agent_icon = Some(agent.icon.clone());
            let resolved_prompt = agent.prompt.replace("{text}", &source_text);
            (
                "custom".to_string(),
                custom_agent_system_prompt().to_string(),
                resolved_prompt,
            )
        }
    };

    let request_id = now_id();
    let mut stream_payload = SelectionResultPayload {
        request_id: request_id.clone(),
        action: action_name.clone(),
        source_text: source_text.clone(),
        output_text: String::new(),
        translate_from: translate_from.clone(),
        translate_to: translate_to.clone(),
        custom_agent_name: custom_agent_name.clone(),
        custom_agent_icon: custom_agent_icon.clone(),
        is_streaming: true,
        error_message: None,
    };

    hide_selection_bar_window(&app);
    show_selection_result_window(&app)?;
    emit_selection_result(&app, stream_payload.clone());

    let mut streamed_output = String::new();
    let mut last_emit_ms = 0u64;
    let stream_base = stream_payload.clone();
    let app_for_stream = app.clone();
    let llm_stream_enabled = true;

    let llm_result = call_llm_for_action(
        &http_client_state.client,
        &snapshot.llm,
        &system_prompt,
        &user_prompt,
        llm_stream_enabled,
        |delta| {
            if !is_selection_result_task_active(&app_for_stream, task_nonce) {
                return false;
            }
            streamed_output.push_str(delta);

            let now = now_epoch_millis();
            if now.saturating_sub(last_emit_ms) < STREAM_EMIT_THROTTLE_MS && !delta.contains('\n') {
                return true;
            }
            last_emit_ms = now;

            let mut update = stream_base.clone();
            update.output_text = streamed_output.clone();
            emit_selection_result(&app_for_stream, update);
            true
        },
    )
    .await;

    if !is_selection_result_task_active(&app, task_nonce) {
        let mut canceled = stream_base.clone();
        canceled.is_streaming = false;
        canceled.output_text = streamed_output;
        canceled.error_message = None;
        return Ok(canceled);
    }

    match llm_result {
        Ok(output_text) => {
            let mut result = stream_base;
            result.output_text = output_text;
            result.is_streaming = false;
            emit_selection_result(&app, result.clone());
            Ok(result)
        }
        Err(error) => {
            if is_task_replaced_error(&error) {
                let mut canceled = stream_base;
                canceled.is_streaming = false;
                canceled.output_text = streamed_output;
                canceled.error_message = None;
                return Ok(canceled);
            }
            stream_payload.is_streaming = false;
            stream_payload.error_message = Some(error.to_string());
            stream_payload.output_text = streamed_output;
            emit_selection_result(&app, stream_payload);
            Err(error)
        }
    }
}

#[tauri::command]
async fn run_ocr_action_cmd(
    app: AppHandle,
    payload: RunOcrActionPayload,
    settings_state: State<'_, AppSettingsState>,
    http_client_state: State<'_, HttpClientState>,
) -> Result<OcrResultPayload, CommandError> {
    let task_nonce = begin_ocr_result_task(&app);
    let ocr_text = payload.ocr_text.trim().to_string();
    if ocr_text.is_empty() {
        return Err(CommandError::Settings("OCR 文本为空".to_string()));
    }

    let snapshot = settings_state
        .data
        .lock()
        .map_err(|_| CommandError::Lock)?
        .clone();

    let mut custom_agent_name: Option<String> = None;
    let mut custom_agent_icon: Option<String> = None;
    let mut translate_from: Option<String> = None;
    let mut translate_to: Option<String> = None;

    let (action_name, system_prompt, user_prompt) = match payload.action {
        SelectionActionKind::Summarize => {
            let prompts = choose_builtin_prompt(
                SelectionActionKind::Summarize,
                &ocr_text,
                None,
                None,
                &snapshot.language,
            );
            ("summary".to_string(), prompts.0, prompts.1)
        }
        SelectionActionKind::Polish => {
            let prompts = choose_builtin_prompt(
                SelectionActionKind::Polish,
                &ocr_text,
                None,
                None,
                &snapshot.language,
            );
            ("polish".to_string(), prompts.0, prompts.1)
        }
        SelectionActionKind::Explain => {
            let prompts = choose_builtin_prompt(
                SelectionActionKind::Explain,
                &ocr_text,
                None,
                None,
                &snapshot.language,
            );
            ("explain".to_string(), prompts.0, prompts.1)
        }
        SelectionActionKind::Translate => {
            translate_from = Some(
                payload
                    .translate_from
                    .clone()
                    .unwrap_or_else(|| "auto".to_string()),
            );
            let default_translate_to =
                if snapshot.selection_assistant.default_translate_to.is_empty() {
                    default_translate_target_for_language(&snapshot.language).to_string()
                } else {
                    snapshot.selection_assistant.default_translate_to.clone()
                };
            let requested_translate_to = payload
                .translate_to
                .clone()
                .unwrap_or_else(|| default_translate_to.clone());
            translate_to = Some(normalize_translate_language(
                &requested_translate_to,
                &default_translate_to,
            ));
            let prompts = choose_builtin_prompt(
                SelectionActionKind::Translate,
                &ocr_text,
                translate_from.as_deref(),
                translate_to.as_deref(),
                &snapshot.language,
            );
            ("translate".to_string(), prompts.0, prompts.1)
        }
        SelectionActionKind::Custom => {
            let custom_id = payload
                .custom_agent_id
                .as_ref()
                .map(|value| value.trim())
                .filter(|value| !value.is_empty())
                .map(|value| value.to_string())
                .or_else(|| {
                    let fallback = snapshot.ocr.custom_agent_id.trim();
                    if fallback.is_empty() {
                        None
                    } else {
                        Some(fallback.to_string())
                    }
                })
                .ok_or_else(|| CommandError::Settings("缺少自定义 Agent ID".to_string()))?;
            let agent = snapshot
                .agents
                .custom
                .iter()
                .find(|item| item.id == custom_id)
                .cloned()
                .ok_or_else(|| CommandError::Settings("未找到可用的自定义 Agent".to_string()))?;
            custom_agent_name = Some(agent.name.clone());
            custom_agent_icon = Some(agent.icon.clone());
            let resolved_prompt = agent.prompt.replace("{text}", &ocr_text);
            (
                "custom".to_string(),
                custom_agent_system_prompt().to_string(),
                resolved_prompt,
            )
        }
    };

    let request_id = now_id();
    let mut stream_payload = OcrResultPayload {
        request_id,
        action: action_name,
        ocr_text: ocr_text.clone(),
        output_text: String::new(),
        translate_from: translate_from.clone(),
        translate_to: translate_to.clone(),
        custom_agent_name: custom_agent_name.clone(),
        custom_agent_icon: custom_agent_icon.clone(),
        is_streaming: true,
        error_message: None,
    };

    show_ocr_result_window(&app)?;
    emit_ocr_result(&app, stream_payload.clone());

    let mut streamed_output = String::new();
    let mut last_emit_ms = 0u64;
    let stream_base = stream_payload.clone();
    let app_for_stream = app.clone();

    let llm_result = call_llm_for_action(
        &http_client_state.client,
        &snapshot.llm,
        &system_prompt,
        &user_prompt,
        true,
        |delta| {
            if !is_ocr_result_task_active(&app_for_stream, task_nonce) {
                return false;
            }
            streamed_output.push_str(delta);

            let now = now_epoch_millis();
            if now.saturating_sub(last_emit_ms) < STREAM_EMIT_THROTTLE_MS && !delta.contains('\n') {
                return true;
            }
            last_emit_ms = now;

            let mut update = stream_base.clone();
            update.output_text = streamed_output.clone();
            emit_ocr_result(&app_for_stream, update);
            true
        },
    )
    .await;

    if !is_ocr_result_task_active(&app, task_nonce) {
        let mut canceled = stream_base.clone();
        canceled.output_text = streamed_output;
        canceled.is_streaming = false;
        canceled.error_message = None;
        return Ok(canceled);
    }

    match llm_result {
        Ok(output_text) => {
            stream_payload.output_text = output_text;
            stream_payload.is_streaming = false;
            emit_ocr_result(&app, stream_payload.clone());
            Ok(stream_payload)
        }
        Err(error) => {
            if is_task_replaced_error(&error) {
                let mut canceled = stream_base;
                canceled.output_text = streamed_output;
                canceled.is_streaming = false;
                canceled.error_message = None;
                return Ok(canceled);
            }
            stream_payload.output_text = streamed_output;
            stream_payload.is_streaming = false;
            if stream_payload.output_text.trim().is_empty() {
                stream_payload.error_message = Some(error.to_string());
                emit_ocr_result(&app, stream_payload.clone());
                emit_ocr_error(&app, &error.to_string());
                Err(error)
            } else {
                stream_payload.error_message = None;
                emit_ocr_result(&app, stream_payload.clone());
                eprintln!(
                    "[run_ocr_action_cmd] stream ended with error after partial output: {}",
                    error
                );
                Ok(stream_payload)
            }
        }
    }
}

#[tauri::command]
async fn synthesize_tts_cmd(
    payload: SynthesizeTtsPayload,
    settings_state: State<'_, AppSettingsState>,
) -> Result<SynthesizeTtsResult, CommandError> {
    let snapshot = settings_state
        .data
        .lock()
        .map_err(|_| CommandError::Lock)?
        .clone();

    let tts = snapshot.tts.clone();
    let text = payload.text;
    let language_hint = payload.language_hint.unwrap_or(snapshot.language);
    let voice_override = payload.voice_override.unwrap_or_default();

    tauri::async_runtime::spawn_blocking(move || {
        let voice_override = voice_override.trim().to_string();
        let override_ref = if voice_override.is_empty() {
            None
        } else {
            Some(voice_override.as_str())
        };
        synthesize_tts_audio(&tts, &text, Some(language_hint.as_str()), override_ref)
    })
    .await
    .map_err(|error| CommandError::Settings(format!("TTS 任务失败: {error}")))?
}

#[tauri::command]
fn start_ocr_capture_cmd(app: AppHandle) -> Result<(), CommandError> {
    start_ocr_capture_workflow(&app)
}

#[tauri::command]
fn cancel_ocr_capture_cmd(
    app: AppHandle,
    ocr_runtime: State<'_, OcrRuntimeState>,
) -> Result<(), CommandError> {
    ocr_runtime.capture_active.store(false, Ordering::Relaxed);
    ocr_runtime
        .suppress_blur_until_ms
        .store(0, Ordering::Relaxed);
    if let Ok(mut snapshot) = ocr_runtime.capture_snapshot.lock() {
        *snapshot = None;
    }
    hide_ocr_capture_window(&app);
    emit_ocr_capture_canceled(&app);
    Ok(())
}

#[tauri::command]
fn get_ocr_result_window_pinned_cmd(
    app: AppHandle,
    settings_state: State<'_, AppSettingsState>,
) -> bool {
    if let Some(window) = app.get_webview_window(OCR_RESULT_WINDOW_LABEL) {
        if let Ok(pinned) = window.is_always_on_top() {
            return pinned;
        }
    }
    settings_state
        .data
        .lock()
        .map(|settings| settings.ocr.result_window_always_on_top)
        .unwrap_or(true)
}

#[tauri::command]
fn set_ocr_result_window_pinned_cmd(app: AppHandle, pinned: bool) -> Result<bool, CommandError> {
    if let Some(window) = app.get_webview_window(OCR_RESULT_WINDOW_LABEL) {
        window
            .set_always_on_top(pinned)
            .map_err(|error| CommandError::Settings(error.to_string()))?;
    }

    Ok(pinned)
}

#[tauri::command]
fn minimize_ocr_result_window_cmd(app: AppHandle) -> Result<(), CommandError> {
    let Some(window) = app.get_webview_window(OCR_RESULT_WINDOW_LABEL) else {
        return Err(CommandError::Settings(
            "OCR result window not found".to_string(),
        ));
    };
    window
        .minimize()
        .map_err(|error| CommandError::Settings(error.to_string()))?;
    Ok(())
}

#[tauri::command]
fn close_ocr_result_window_cmd(app: AppHandle) -> Result<(), CommandError> {
    let Some(window) = app.get_webview_window(OCR_RESULT_WINDOW_LABEL) else {
        return Err(CommandError::Settings(
            "OCR result window not found".to_string(),
        ));
    };
    window
        .hide()
        .map_err(|error| CommandError::Settings(error.to_string()))?;
    Ok(())
}

#[tauri::command]
async fn complete_ocr_capture_cmd(
    app: AppHandle,
    area: OcrCaptureAreaPayload,
    settings_state: State<'_, AppSettingsState>,
    ocr_runtime: State<'_, OcrRuntimeState>,
    http_client_state: State<'_, HttpClientState>,
) -> Result<(), CommandError> {
    let task_nonce = begin_ocr_result_task(&app);
    ocr_runtime.capture_active.store(false, Ordering::Relaxed);
    ocr_runtime
        .suppress_blur_until_ms
        .store(0, Ordering::Relaxed);

    let snapshot = settings_state
        .data
        .lock()
        .map_err(|_| CommandError::Lock)?
        .clone();
    if !snapshot.ocr.enabled {
        hide_ocr_capture_window(&app);
        if let Ok(mut capture_snapshot) = ocr_runtime.capture_snapshot.lock() {
            *capture_snapshot = None;
        }
        return Err(CommandError::Settings(
            "请先在设置中启用智能 OCR".to_string(),
        ));
    }

    let image_data_url = capture_ocr_area_to_data_url(&app, &ocr_runtime, &area)?;
    if let Ok(mut capture_snapshot) = ocr_runtime.capture_snapshot.lock() {
        *capture_snapshot = None;
    }
    hide_ocr_capture_window(&app);
    sleep_for_ms(8).await;

    let request_id = now_id();
    let mut ocr_payload = OcrResultPayload {
        request_id,
        action: match snapshot.ocr.default_action {
            OcrDefaultAction::Translate => "translate",
            OcrDefaultAction::Summarize => "summary",
            OcrDefaultAction::Polish => "polish",
            OcrDefaultAction::Explain => "explain",
            OcrDefaultAction::Custom => "custom",
        }
        .to_string(),
        ocr_text: String::new(),
        output_text: String::new(),
        translate_from: None,
        translate_to: None,
        custom_agent_name: None,
        custom_agent_icon: None,
        is_streaming: true,
        error_message: None,
    };

    show_ocr_result_window(&app)?;
    emit_ocr_result(&app, ocr_payload.clone());

    let ocr_text = match call_vision_ocr(
        &http_client_state.client,
        &snapshot.ocr.vision,
        &image_data_url,
    )
    .await
    {
        Ok(text) => text,
        Err(error) => {
            ocr_payload.is_streaming = false;
            ocr_payload.error_message = Some(error.to_string());
            emit_ocr_result(&app, ocr_payload);
            emit_ocr_error(&app, &error.to_string());
            return Err(error);
        }
    };
    if !is_ocr_result_task_active(&app, task_nonce) {
        return Ok(());
    }

    ocr_payload.ocr_text = ocr_text.clone();
    emit_ocr_result(&app, ocr_payload.clone());

    if !snapshot.ocr.auto_run_after_capture {
        ocr_payload.is_streaming = false;
        ocr_payload.output_text = ocr_text;
        emit_ocr_result(&app, ocr_payload);
        return Ok(());
    }

    let mut custom_agent_name: Option<String> = None;
    let mut custom_agent_icon: Option<String> = None;
    let mut translate_from: Option<String> = None;
    let mut translate_to: Option<String> = None;

    let (action_name, system_prompt, user_prompt) = match snapshot.ocr.default_action {
        OcrDefaultAction::Summarize => {
            let prompts = choose_builtin_prompt(
                SelectionActionKind::Summarize,
                &ocr_text,
                None,
                None,
                &snapshot.language,
            );
            ("summary".to_string(), prompts.0, prompts.1)
        }
        OcrDefaultAction::Polish => {
            let prompts = choose_builtin_prompt(
                SelectionActionKind::Polish,
                &ocr_text,
                None,
                None,
                &snapshot.language,
            );
            ("polish".to_string(), prompts.0, prompts.1)
        }
        OcrDefaultAction::Explain => {
            let prompts = choose_builtin_prompt(
                SelectionActionKind::Explain,
                &ocr_text,
                None,
                None,
                &snapshot.language,
            );
            ("explain".to_string(), prompts.0, prompts.1)
        }
        OcrDefaultAction::Translate => {
            translate_from = Some("auto".to_string());
            translate_to = Some(
                if snapshot.selection_assistant.default_translate_to.is_empty() {
                    default_translate_target_for_language(&snapshot.language).to_string()
                } else {
                    snapshot.selection_assistant.default_translate_to.clone()
                },
            );
            let prompts = choose_builtin_prompt(
                SelectionActionKind::Translate,
                &ocr_text,
                translate_from.as_deref(),
                translate_to.as_deref(),
                &snapshot.language,
            );
            ("translate".to_string(), prompts.0, prompts.1)
        }
        OcrDefaultAction::Custom => {
            let custom_id = snapshot.ocr.custom_agent_id.trim();
            let agent = snapshot
                .agents
                .custom
                .iter()
                .find(|item| item.id == custom_id)
                .or_else(|| snapshot.agents.custom.first())
                .cloned()
                .ok_or_else(|| CommandError::Settings("未找到可用的自定义 Agent".to_string()))?;
            custom_agent_name = Some(agent.name.clone());
            custom_agent_icon = Some(agent.icon.clone());
            let resolved_prompt = agent.prompt.replace("{text}", &ocr_text);
            (
                "custom".to_string(),
                custom_agent_system_prompt().to_string(),
                resolved_prompt,
            )
        }
    };

    let mut stream_payload = OcrResultPayload {
        request_id: ocr_payload.request_id.clone(),
        action: action_name,
        ocr_text,
        output_text: String::new(),
        translate_from,
        translate_to,
        custom_agent_name,
        custom_agent_icon,
        is_streaming: true,
        error_message: None,
    };
    emit_ocr_result(&app, stream_payload.clone());

    let mut streamed_output = String::new();
    let mut last_emit_ms = 0u64;
    let stream_base = stream_payload.clone();
    let app_for_stream = app.clone();
    let llm_stream_enabled = true;

    let llm_result = call_llm_for_action(
        &http_client_state.client,
        &snapshot.llm,
        &system_prompt,
        &user_prompt,
        llm_stream_enabled,
        |delta| {
            if !is_ocr_result_task_active(&app_for_stream, task_nonce) {
                return false;
            }
            streamed_output.push_str(delta);
            let now = now_epoch_millis();
            if now.saturating_sub(last_emit_ms) < STREAM_EMIT_THROTTLE_MS && !delta.contains('\n') {
                return true;
            }
            last_emit_ms = now;
            let mut update = stream_base.clone();
            update.output_text = streamed_output.clone();
            emit_ocr_result(&app_for_stream, update);
            true
        },
    )
    .await;

    if !is_ocr_result_task_active(&app, task_nonce) {
        return Ok(());
    }

    match llm_result {
        Ok(output_text) => {
            stream_payload.output_text = output_text;
            stream_payload.is_streaming = false;
            emit_ocr_result(&app, stream_payload);
            Ok(())
        }
        Err(error) => {
            if is_task_replaced_error(&error) {
                return Ok(());
            }
            stream_payload.output_text = streamed_output;
            stream_payload.is_streaming = false;
            if stream_payload.output_text.trim().is_empty() {
                stream_payload.error_message = Some(error.to_string());
                emit_ocr_result(&app, stream_payload);
                emit_ocr_error(&app, &error.to_string());
                Err(error)
            } else {
                stream_payload.error_message = None;
                emit_ocr_result(&app, stream_payload);
                eprintln!(
                    "[complete_ocr_capture_cmd] stream ended with error after partial output: {}",
                    error
                );
                Ok(())
            }
        }
    }
}

#[tauri::command]
fn show_main_window_cmd(app: AppHandle) {
    show_main_window(&app);
}

#[tauri::command]
fn hide_main_window_cmd(app: AppHandle) {
    hide_window_by_label(&app, MAIN_WINDOW_LABEL);
}

#[tauri::command]
fn start_main_window_drag_cmd(flags: State<'_, RuntimeFlags>) -> Result<(), CommandError> {
    flags
        .suppress_auto_hide_until_ms
        .store(now_epoch_millis() + 1200, Ordering::Relaxed);

    Ok(())
}

#[tauri::command]
fn set_main_window_pinned_cmd(
    app: AppHandle,
    pinned: bool,
    flags: State<'_, RuntimeFlags>,
) -> Result<bool, CommandError> {
    if let Some(window) = app.get_webview_window(MAIN_WINDOW_LABEL) {
        window
            .set_always_on_top(pinned)
            .map_err(|error| CommandError::Settings(error.to_string()))?;
    }

    flags.main_window_pinned.store(pinned, Ordering::Relaxed);
    Ok(pinned)
}

#[tauri::command]
fn get_main_window_pinned_cmd(flags: State<'_, RuntimeFlags>) -> bool {
    flags.main_window_pinned.load(Ordering::Relaxed)
}

#[tauri::command]
fn get_settings(settings_state: State<'_, AppSettingsState>) -> Result<AppSettings, CommandError> {
    let settings = with_settings_lock(&settings_state)?;
    Ok(settings.clone())
}

#[tauri::command]
async fn test_llm_api_cmd(
    settings_state: State<'_, AppSettingsState>,
    http_client_state: State<'_, HttpClientState>,
) -> Result<String, CommandError> {
    let snapshot = settings_state
        .data
        .lock()
        .map_err(|_| CommandError::Lock)?
        .clone();

    if snapshot.llm.api_key.trim().is_empty() {
        return Err(CommandError::Settings("请先填写大模型 API Key".to_string()));
    }

    test_openai_compatible_model(
        &http_client_state.client,
        &snapshot.llm.base_url,
        &snapshot.llm.api_key,
        &snapshot.llm.model,
        snapshot.llm.timeout_ms,
    )
    .await
}

#[tauri::command]
async fn test_ocr_vision_api_cmd(
    settings_state: State<'_, AppSettingsState>,
    http_client_state: State<'_, HttpClientState>,
) -> Result<String, CommandError> {
    let snapshot = settings_state
        .data
        .lock()
        .map_err(|_| CommandError::Lock)?
        .clone();
    let vision = snapshot.ocr.vision;

    if vision.api_key.trim().is_empty() {
        return Err(CommandError::Settings(
            "请先填写 OCR 视觉模型 API Key".to_string(),
        ));
    }

    if should_use_glm_layout_parsing(&vision) {
        let text =
            call_glm_layout_parsing_ocr(&http_client_state.client, &vision, GLM_OCR_TEST_IMAGE_URL)
                .await?;
        let normalized = text.trim();
        if normalized.is_empty() {
            return Ok("OK".to_string());
        }
        return Ok(normalized.chars().take(80).collect());
    }

    test_openai_compatible_model(
        &http_client_state.client,
        &vision.base_url,
        &vision.api_key,
        &vision.model,
        vision.timeout_ms,
    )
    .await
}

#[tauri::command]
fn pick_history_storage_folder() -> Result<Option<String>, CommandError> {
    let selected = FileDialog::new()
        .set_title("选择复制内容存储文件夹")
        .pick_folder();
    Ok(selected.map(|path| path.to_string_lossy().into_owned()))
}

#[tauri::command]
fn update_settings(
    app: AppHandle,
    patch: SettingsPatch,
    settings_state: State<'_, AppSettingsState>,
    history_state: State<'_, Mutex<ClipboardState>>,
) -> Result<AppSettings, CommandError> {
    update_settings_internal(&app, &settings_state, &history_state, patch)
}

#[tauri::command]
fn reset_settings(
    app: AppHandle,
    settings_state: State<'_, AppSettingsState>,
    history_state: State<'_, Mutex<ClipboardState>>,
) -> Result<AppSettings, CommandError> {
    let defaults = AppSettings::default();
    let patch = SettingsPatch {
        theme_preset: Some(defaults.theme_preset),
        language: Some(defaults.language.clone()),
        window: Some(WindowSettingsPatch {
            auto_hide_on_blur: Some(defaults.window.auto_hide_on_blur),
            remember_position: Some(defaults.window.remember_position),
            remember_main_window_size: Some(defaults.window.remember_main_window_size),
            launch_on_system_startup: Some(defaults.window.launch_on_system_startup),
            silent_startup: Some(defaults.window.silent_startup),
            check_updates_on_startup: Some(defaults.window.check_updates_on_startup),
        }),
        selection_assistant: Some(SelectionAssistantSettingsPatch {
            enabled: Some(defaults.selection_assistant.enabled),
            mode: Some(defaults.selection_assistant.mode),
            show_icon_animation: Some(defaults.selection_assistant.show_icon_animation),
            compact_mode: Some(defaults.selection_assistant.compact_mode),
            auto_hide_ms: Some(defaults.selection_assistant.auto_hide_ms),
            search_url_template: Some(defaults.selection_assistant.search_url_template.clone()),
            min_chars: Some(defaults.selection_assistant.min_chars),
            max_chars: Some(defaults.selection_assistant.max_chars),
            blocked_apps: Some(defaults.selection_assistant.blocked_apps.clone()),
            default_translate_to: Some(defaults.selection_assistant.default_translate_to.clone()),
            result_window_always_on_top: Some(
                defaults.selection_assistant.result_window_always_on_top,
            ),
            remember_result_window_position: Some(
                defaults.selection_assistant.remember_result_window_position,
            ),
        }),
        llm: Some(LlmSettingsPatch {
            enabled: Some(defaults.llm.enabled),
            base_url: Some(defaults.llm.base_url.clone()),
            api_key: Some(defaults.llm.api_key.clone()),
            model: Some(defaults.llm.model.clone()),
            temperature: Some(defaults.llm.temperature),
            max_tokens: Some(defaults.llm.max_tokens),
            timeout_ms: Some(defaults.llm.timeout_ms),
        }),
        tts: Some(TtsSettingsPatch {
            runtime_mode: Some(defaults.tts.runtime_mode),
            voice_zh_cn: Some(defaults.tts.voice_zh_cn.clone()),
            voice_en_us: Some(defaults.tts.voice_en_us.clone()),
            rate_percent: Some(defaults.tts.rate_percent),
        }),
        agents: Some(AgentSettingsPatch {
            custom: Some(defaults.agents.custom.clone()),
            bar_order: Some(defaults.agents.bar_order.clone()),
        }),
        shortcuts: Some(ShortcutSettingsPatch {
            toggle_main: Some(defaults.shortcuts.toggle_main.clone()),
            toggle_ocr: Some(defaults.shortcuts.toggle_ocr.clone()),
        }),
        ocr: Some(OcrSettingsPatch {
            enabled: Some(defaults.ocr.enabled),
            auto_run_after_capture: Some(defaults.ocr.auto_run_after_capture),
            default_action: Some(defaults.ocr.default_action),
            custom_agent_id: Some(defaults.ocr.custom_agent_id.clone()),
            result_window_always_on_top: Some(defaults.ocr.result_window_always_on_top),
            remember_result_window_position: Some(defaults.ocr.remember_result_window_position),
            vision: Some(VisionSettingsPatch {
                enabled: Some(defaults.ocr.vision.enabled),
                base_url: Some(defaults.ocr.vision.base_url.clone()),
                api_key: Some(defaults.ocr.vision.api_key.clone()),
                model: Some(defaults.ocr.vision.model.clone()),
                temperature: Some(defaults.ocr.vision.temperature),
                max_tokens: Some(defaults.ocr.vision.max_tokens),
                timeout_ms: Some(defaults.ocr.vision.timeout_ms),
            }),
        }),
        history: Some(HistorySettingsPatch {
            poll_ms: Some(defaults.history.poll_ms),
            max_items: Some(defaults.history.max_items),
            dedupe: Some(defaults.history.dedupe),
            capture_text: Some(defaults.history.capture_text),
            capture_link: Some(defaults.history.capture_link),
            capture_image: Some(defaults.history.capture_image),
            default_open_category: Some(defaults.history.default_open_category),
            default_category: Some(defaults.history.default_category),
            paste_behavior: Some(defaults.history.paste_behavior),
            collapse_top_bar: Some(defaults.history.collapse_top_bar),
            promote_after_paste: Some(defaults.history.promote_after_paste),
            open_at_top_on_show: Some(defaults.history.open_at_top_on_show),
            storage_path: Some(defaults.history.storage_path.clone()),
        }),
        selection_result_window_width: defaults.selection_result_window_width,
        selection_result_window_height: defaults.selection_result_window_height,
        ocr_result_window_width: defaults.ocr_result_window_width,
        ocr_result_window_height: defaults.ocr_result_window_height,
    };

    update_settings_internal(&app, &settings_state, &history_state, patch)
}

#[tauri::command]
fn set_toggle_shortcut(
    app: AppHandle,
    shortcut: String,
    settings_state: State<'_, AppSettingsState>,
    history_state: State<'_, Mutex<ClipboardState>>,
) -> Result<AppSettings, CommandError> {
    let patch = SettingsPatch {
        shortcuts: Some(ShortcutSettingsPatch {
            toggle_main: Some(shortcut),
            ..ShortcutSettingsPatch::default()
        }),
        ..SettingsPatch::default()
    };

    update_settings_internal(&app, &settings_state, &history_state, patch)
}

#[tauri::command]
fn set_toggle_ocr_shortcut(
    app: AppHandle,
    shortcut: String,
    settings_state: State<'_, AppSettingsState>,
    history_state: State<'_, Mutex<ClipboardState>>,
) -> Result<AppSettings, CommandError> {
    let patch = SettingsPatch {
        shortcuts: Some(ShortcutSettingsPatch {
            toggle_ocr: Some(shortcut),
            ..ShortcutSettingsPatch::default()
        }),
        ..SettingsPatch::default()
    };

    update_settings_internal(&app, &settings_state, &history_state, patch)
}

#[tauri::command]
fn export_settings(settings_state: State<'_, AppSettingsState>) -> Result<String, CommandError> {
    let settings = with_settings_lock(&settings_state)?;
    serde_json::to_string_pretty(&*settings)
        .map_err(|error| CommandError::Serialization(error.to_string()))
}

#[tauri::command]
fn import_settings(
    app: AppHandle,
    payload: String,
    settings_state: State<'_, AppSettingsState>,
    history_state: State<'_, Mutex<ClipboardState>>,
) -> Result<AppSettings, CommandError> {
    let mut incoming = serde_json::from_str::<AppSettings>(&payload)
        .map_err(|error| CommandError::Serialization(error.to_string()))?;
    normalize_settings(&mut incoming);

    let (previous_settings, updated_settings) = {
        let mut settings = with_settings_lock(&settings_state)?;
        let previous = settings.clone();
        *settings = incoming;
        (previous, settings.clone())
    };

    if let Err(error) = apply_shortcut_change(
        &app,
        &previous_settings.shortcuts.toggle_main,
        &updated_settings.shortcuts.toggle_main,
        ShortcutAction::ToggleMain,
    ) {
        if let Ok(mut settings) = with_settings_lock(&settings_state) {
            *settings = previous_settings;
        }
        return Err(error);
    }
    if let Err(error) = apply_shortcut_change(
        &app,
        &previous_settings.shortcuts.toggle_ocr,
        &updated_settings.shortcuts.toggle_ocr,
        ShortcutAction::StartOcrCapture,
    ) {
        let _ = apply_shortcut_change(
            &app,
            &updated_settings.shortcuts.toggle_main,
            &previous_settings.shortcuts.toggle_main,
            ShortcutAction::ToggleMain,
        );
        if let Ok(mut settings) = with_settings_lock(&settings_state) {
            *settings = previous_settings;
        }
        return Err(error);
    }

    if settings_state.file_path.exists() {
        let backup = settings_backup_path(&settings_state.file_path);
        let _ = fs::copy(&settings_state.file_path, backup);
    }

    persist_settings(&settings_state.file_path, &updated_settings)?;
    trim_history_by_settings(&history_state, &updated_settings)?;
    {
        let history_snapshot = with_history_lock(&history_state)?
            .history
            .iter()
            .cloned()
            .collect::<Vec<_>>();
        persist_history_snapshot(&app, &updated_settings, &history_snapshot)?;
    }

    if let Some(window) = app.get_webview_window(SELECTION_RESULT_WINDOW_LABEL) {
        let _ = window.set_always_on_top(
            updated_settings
                .selection_assistant
                .result_window_always_on_top,
        );
    }
    if let Some(window) = app.get_webview_window(OCR_RESULT_WINDOW_LABEL) {
        let _ = window.set_always_on_top(updated_settings.ocr.result_window_always_on_top);
    }
    if !updated_settings.selection_assistant.enabled {
        hide_selection_bar_window(&app);
    }
    if !updated_settings.ocr.enabled {
        hide_ocr_capture_window(&app);
    }

    emit_settings_updated(&app, &updated_settings);

    Ok(updated_settings)
}

#[tauri::command]
fn set_auto_hide_on_blur(
    app: AppHandle,
    enabled: bool,
    settings_state: State<'_, AppSettingsState>,
    history_state: State<'_, Mutex<ClipboardState>>,
) -> Result<AppSettings, CommandError> {
    let patch = SettingsPatch {
        window: Some(WindowSettingsPatch {
            auto_hide_on_blur: Some(enabled),
            ..WindowSettingsPatch::default()
        }),
        ..SettingsPatch::default()
    };

    update_settings_internal(&app, &settings_state, &history_state, patch)
}

#[tauri::command]
fn get_auto_hide_on_blur(
    settings_state: State<'_, AppSettingsState>,
) -> Result<bool, CommandError> {
    let settings = with_settings_lock(&settings_state)?;
    Ok(settings.window.auto_hide_on_blur)
}

#[tauri::command]
fn get_selection_assistant_status(
    settings_state: State<'_, AppSettingsState>,
) -> Result<SelectionAssistantSettings, CommandError> {
    let settings = with_settings_lock(&settings_state)?;
    Ok(settings.selection_assistant.clone())
}

#[tauri::command]
fn set_selection_assistant_enabled(
    app: AppHandle,
    enabled: bool,
    settings_state: State<'_, AppSettingsState>,
    history_state: State<'_, Mutex<ClipboardState>>,
) -> Result<AppSettings, CommandError> {
    let patch = SettingsPatch {
        selection_assistant: Some(SelectionAssistantSettingsPatch {
            enabled: Some(enabled),
            ..SelectionAssistantSettingsPatch::default()
        }),
        ..SettingsPatch::default()
    };
    update_settings_internal(&app, &settings_state, &history_state, patch)
}

#[tauri::command]
fn sync_clipboard(
    app: AppHandle,
    settings_state: State<'_, AppSettingsState>,
    state: State<'_, Mutex<ClipboardState>>,
) -> Result<Option<Vec<ClipboardEntry>>, CommandError> {
    let settings_snapshot = {
        let settings = with_settings_lock(&settings_state)?;
        settings.clone()
    };

    let current_sequence = clipboard_sequence_number();
    if current_sequence != 0 {
        let locked = with_history_lock(&state)?;
        if locked.last_clipboard_sequence == Some(current_sequence) {
            return Ok(None);
        }
    }

    let mut clipboard =
        Clipboard::new().map_err(|error| CommandError::Clipboard(error.to_string()))?;
    let mut incoming: Option<ClipboardEntry> = None;

    if settings_snapshot.history.capture_image {
        if let Ok(image) = clipboard.get_image() {
            incoming = Some(build_image_entry(image)?);
        }
    }

    if incoming.is_none()
        && (settings_snapshot.history.capture_text || settings_snapshot.history.capture_link)
    {
        match clipboard.get_text() {
            Ok(text) => {
                let content = text.trim().to_string();
                if !content.is_empty() {
                    let candidate = build_text_entry(content);
                    if capture_kind_allowed(candidate.kind, &settings_snapshot.history) {
                        incoming = Some(candidate);
                    }
                }
            }
            Err(arboard::Error::ContentNotAvailable) => {}
            Err(arboard::Error::ClipboardOccupied) => {}
            Err(error) => return Err(CommandError::Clipboard(error.to_string())),
        }
    }

    let mut locked = with_history_lock(&state)?;
    let mut changed = false;

    if let Some(entry) = incoming {
        let signature = entry_signature(&entry);
        if locked.last_observed_signature != Some(signature) {
            insert_or_promote(
                &mut locked.history,
                entry,
                settings_snapshot.history.max_items,
                settings_snapshot.history.dedupe,
            );
            locked.last_observed_signature = Some(signature);
            changed = true;
        }
    } else if locked.last_observed_signature.is_some() {
        locked.last_observed_signature = None;
    }
    if current_sequence != 0 {
        locked.last_clipboard_sequence = Some(current_sequence);
    }

    let updated = if changed {
        Some(collect_history(&locked.history))
    } else {
        None
    };
    drop(locked);
    if let Some(items) = updated.as_ref() {
        persist_history_snapshot(&app, &settings_snapshot, items)?;
        emit_history_updated(&app, items);
    }
    Ok(updated)
}

#[tauri::command]
fn get_history(
    app: AppHandle,
    settings_state: State<'_, AppSettingsState>,
    state: State<'_, Mutex<ClipboardState>>,
) -> Result<Vec<ClipboardEntry>, CommandError> {
    {
        let locked = with_history_lock(&state)?;
        if !locked.history.is_empty() {
            return Ok(collect_history(&locked.history));
        }
    }

    let settings_snapshot = with_settings_lock(&settings_state)?.clone();
    let restored = load_history_snapshot(
        &app,
        &settings_snapshot,
        &settings_state.file_path,
        &settings_state.file_path,
    )?;

    if !restored.history.is_empty() {
        let restored_items = restored.history.iter().cloned().collect::<Vec<_>>();
        {
            let mut locked = with_history_lock(&state)?;
            if locked.history.is_empty() {
                locked.history = restored.history.clone();
                locked.last_observed_signature = None;
            }
        }
        if restored.should_persist {
            let _ = persist_history_snapshot(&app, &settings_snapshot, &restored_items);
        }
        return Ok(restored_items);
    }

    let locked = with_history_lock(&state)?;
    Ok(collect_history(&locked.history))
}

#[tauri::command]
fn set_last_opened_category_cmd(
    app: AppHandle,
    category: FilterKind,
    settings_state: State<'_, AppSettingsState>,
) -> Result<FilterKind, CommandError> {
    let mut changed = false;
    {
        let mut settings = with_settings_lock(&settings_state)?;
        if settings.history.default_category != category {
            settings.history.default_category = category;
            changed = true;
        }
    }

    if !changed {
        return Ok(category);
    }

    persist_settings_state(&settings_state)?;

    let snapshot = with_settings_lock(&settings_state)?.clone();
    emit_settings_updated(&app, &snapshot);
    Ok(category)
}

#[tauri::command]
fn paste_entry_by_click(
    id: String,
    app: AppHandle,
    settings_state: State<'_, AppSettingsState>,
    state: State<'_, Mutex<ClipboardState>>,
) -> Result<Vec<ClipboardEntry>, CommandError> {
    let settings_snapshot = {
        let settings = with_settings_lock(&settings_state)?;
        settings.clone()
    };

    let mut locked = with_history_lock(&state)?;
    let target = locked
        .history
        .iter()
        .find(|entry| entry.id == id)
        .cloned()
        .ok_or(CommandError::NotFound)?;

    let mut clipboard =
        Clipboard::new().map_err(|error| CommandError::Clipboard(error.to_string()))?;
    apply_entry_to_clipboard(&mut clipboard, &target)?;

    let updated = if settings_snapshot.history.promote_after_paste {
        insert_or_promote(
            &mut locked.history,
            target.clone(),
            settings_snapshot.history.max_items,
            settings_snapshot.history.dedupe,
        );
        locked.last_observed_signature = Some(entry_signature(&target));
        let updated = collect_history(&locked.history);
        drop(locked);
        persist_history_snapshot(&app, &settings_snapshot, &updated)?;
        updated
    } else {
        locked.last_observed_signature = Some(entry_signature(&target));
        let updated = collect_history(&locked.history);
        drop(locked);
        updated
    };

    let runtime_flags = app.state::<RuntimeFlags>();
    let pinned_by_flag = runtime_flags.main_window_pinned.load(Ordering::Relaxed);
    let target_hwnd = runtime_flags.last_foreground_hwnd.load(Ordering::Relaxed);
    let pinned_by_window = app
        .get_webview_window(MAIN_WINDOW_LABEL)
        .and_then(|window| window.is_always_on_top().ok())
        .unwrap_or(false);
    let is_main_window_pinned = pinned_by_flag || pinned_by_window;

    let should_hide_after_copy = matches!(
        settings_snapshot.history.paste_behavior,
        PasteBehavior::CopyAndHide
    ) && !is_main_window_pinned;

    if should_hide_after_copy {
        if let Some(window) = app.get_webview_window(MAIN_WINDOW_LABEL) {
            let _ = window.hide();
        }
        if let Some(global_settings_state) = app.try_state::<AppSettingsState>() {
            let _ = persist_settings_state(&global_settings_state);
        }
    }

    std::thread::spawn(move || {
        std::thread::sleep(Duration::from_millis(90));
        let _ = send_system_paste_shortcut(target_hwnd);
    });

    emit_history_updated(&app, &updated);
    Ok(updated)
}

#[tauri::command]
fn toggle_pin(
    id: String,
    app: AppHandle,
    settings_state: State<'_, AppSettingsState>,
    state: State<'_, Mutex<ClipboardState>>,
) -> Result<Vec<ClipboardEntry>, CommandError> {
    let settings_snapshot = {
        let settings = with_settings_lock(&settings_state)?;
        settings.clone()
    };

    let mut locked = with_history_lock(&state)?;
    if let Some(entry) = locked.history.iter_mut().find(|entry| entry.id == id) {
        entry.pinned = !entry.pinned;
        normalize_history_order(&mut locked.history);
        let updated = collect_history(&locked.history);
        drop(locked);
        persist_history_snapshot(&app, &settings_snapshot, &updated)?;
        emit_history_updated(&app, &updated);
        return Ok(updated);
    }
    Err(CommandError::NotFound)
}

#[tauri::command]
fn toggle_favorite_text_cmd(
    text: String,
    app: AppHandle,
    settings_state: State<'_, AppSettingsState>,
    state: State<'_, Mutex<ClipboardState>>,
) -> Result<bool, CommandError> {
    let content = text.trim().to_string();
    if content.is_empty() {
        return Ok(false);
    }

    let settings_snapshot = {
        let settings = with_settings_lock(&settings_state)?;
        settings.clone()
    };

    let mut locked = with_history_lock(&state)?;
    let match_indices = locked
        .history
        .iter()
        .enumerate()
        .filter_map(|(index, entry)| {
            if matches!(entry.kind, ClipboardKind::Text | ClipboardKind::Link)
                && entry.content == content
            {
                Some(index)
            } else {
                None
            }
        })
        .collect::<Vec<_>>();

    if !match_indices.is_empty() {
        let has_pinned = match_indices.iter().any(|index| {
            locked
                .history
                .get(*index)
                .map(|entry| entry.pinned)
                .unwrap_or(false)
        });
        let next_pinned = !has_pinned;
        for index in match_indices {
            if let Some(entry) = locked.history.get_mut(index) {
                entry.pinned = next_pinned;
            }
        }
        normalize_history_order(&mut locked.history);
        let updated = collect_history(&locked.history);
        drop(locked);
        persist_history_snapshot(&app, &settings_snapshot, &updated)?;
        emit_history_updated(&app, &updated);
        return Ok(next_pinned);
    }

    let mut incoming = build_text_entry(content);
    incoming.pinned = true;
    let incoming_signature = entry_signature(&incoming);
    insert_or_promote(
        &mut locked.history,
        incoming,
        settings_snapshot.history.max_items,
        settings_snapshot.history.dedupe,
    );
    locked.last_observed_signature = Some(incoming_signature);
    let updated = collect_history(&locked.history);
    drop(locked);

    persist_history_snapshot(&app, &settings_snapshot, &updated)?;
    emit_history_updated(&app, &updated);
    Ok(true)
}

#[tauri::command]
fn remove_item(
    id: String,
    app: AppHandle,
    settings_state: State<'_, AppSettingsState>,
    state: State<'_, Mutex<ClipboardState>>,
) -> Result<Vec<ClipboardEntry>, CommandError> {
    let settings_snapshot = {
        let settings = with_settings_lock(&settings_state)?;
        settings.clone()
    };

    let mut locked = with_history_lock(&state)?;
    if let Some(idx) = locked.history.iter().position(|entry| entry.id == id) {
        locked.history.remove(idx);
        let updated = collect_history(&locked.history);
        drop(locked);
        persist_history_snapshot(&app, &settings_snapshot, &updated)?;
        emit_history_updated(&app, &updated);
        return Ok(updated);
    }
    Err(CommandError::NotFound)
}

#[tauri::command]
fn clear_history(
    app: AppHandle,
    settings_state: State<'_, AppSettingsState>,
    state: State<'_, Mutex<ClipboardState>>,
) -> Result<Vec<ClipboardEntry>, CommandError> {
    let settings_snapshot = {
        let settings = with_settings_lock(&settings_state)?;
        settings.clone()
    };

    let mut locked = with_history_lock(&state)?;
    locked.history.retain(|entry| entry.pinned);
    let updated = collect_history(&locked.history);
    drop(locked);
    persist_history_snapshot(&app, &settings_snapshot, &updated)?;
    emit_history_updated(&app, &updated);
    Ok(updated)
}

pub fn run() {
    tauri::Builder::default()
        .manage(Mutex::new(ClipboardState::default()))
        .manage(RuntimeFlags::default())
        .manage(SelectionRuntimeState::default())
        .manage(OcrRuntimeState::default())
        .manage(HttpClientState::default())
        .setup(|app| {
            let app_handle = app.handle().clone();
            let path = settings_file_path(&app_handle)?;
            let settings_load = load_settings(&path);
            let settings = settings_load.settings.clone();
            let is_autostart_launch =
                std::env::args().any(|arg| arg.eq_ignore_ascii_case(AUTOSTART_ARG));

            if settings_load.should_persist {
                if settings_load.source_path != path {
                    eprintln!(
                        "[Settings] recovered from {}",
                        settings_load.source_path.display()
                    );
                }
                let _ = persist_settings(&path, &settings);
            }
            let history_load = match load_history_snapshot(
                &app_handle,
                &settings,
                &path,
                &settings_load.source_path,
            ) {
                Ok(result) => result,
                Err(error) => {
                    eprintln!("[History] startup load failed: {error}");
                    HistoryLoadResult {
                        history: VecDeque::new(),
                        source_path: None,
                        should_persist: false,
                    }
                }
            };
            let initial_history = history_load.history.clone();

            app.manage(AppSettingsState {
                file_path: path,
                data: Mutex::new(settings.clone()),
            });
            {
                let history_state = app_handle.state::<Mutex<ClipboardState>>();
                if let Ok(mut history) = history_state.lock() {
                    history.history = initial_history.clone();
                };
            }
            if history_load.should_persist {
                if let Some(source_path) = history_load.source_path.as_ref() {
                    eprintln!(
                        "[History] migrated snapshot from {}",
                        source_path.display()
                    );
                }
                let _ = persist_history_snapshot(
                    &app_handle,
                    &settings,
                    &initial_history.iter().cloned().collect::<Vec<_>>(),
                );
            }

            let initially_pinned = app_handle
                .get_webview_window(MAIN_WINDOW_LABEL)
                .and_then(|window| window.is_always_on_top().ok())
                .unwrap_or(false);
            let runtime_flags = app_handle.state::<RuntimeFlags>();
            runtime_flags
                .main_window_pinned
                .store(initially_pinned, Ordering::Relaxed);

            if let Some(result_window) = app_handle.get_webview_window(SELECTION_RESULT_WINDOW_LABEL) {
                let _ = result_window
                    .set_always_on_top(settings.selection_assistant.result_window_always_on_top);
            }
            if let Some(result_window) = app_handle.get_webview_window(OCR_RESULT_WINDOW_LABEL) {
                let _ = result_window.set_always_on_top(settings.ocr.result_window_always_on_top);
            }
            apply_result_windows_background(&app_handle, &settings);
            if let Err(error) = sync_autostart_with_settings(&app_handle, &settings) {
                eprintln!("Failed to sync auto-start state: {error}");
            }
            schedule_autostart_sync_retry(app_handle.clone());

            if let Err(error) = create_tray(&app_handle) {
                eprintln!("Failed to create tray icon: {error}");
            }

            if let Err(error) = register_or_replace_shortcut(
                &app_handle,
                None,
                &settings.shortcuts.toggle_main,
                ShortcutAction::ToggleMain,
            ) {
                eprintln!(
                    "Failed to register shortcut {}: {error}. Shortcut may be occupied by another app.",
                    settings.shortcuts.toggle_main
                );
            }
            if let Err(error) = register_or_replace_shortcut(
                &app_handle,
                None,
                &settings.shortcuts.toggle_ocr,
                ShortcutAction::StartOcrCapture,
            ) {
                eprintln!(
                    "Failed to register OCR shortcut {}: {error}. Shortcut may be occupied by another app.",
                    settings.shortcuts.toggle_ocr
                );
            }

            start_selection_detector_thread(app_handle.clone());

            std::thread::spawn(|| {
                if has_edge_tts_runtime() {
                    EDGE_TTS_AUTO_INSTALL_ATTEMPTED.store(true, Ordering::Relaxed);
                    return;
                }

                if let Err(error) = try_auto_install_edge_tts_with_lock() {
                    if error != EDGE_TTS_INSTALL_IN_PROGRESS {
                        eprintln!("[TTS bootstrap] Edge TTS auto install failed: {error}");
                    }
                }
            });

            if is_autostart_launch && !settings.window.silent_startup {
                show_settings_window(&app_handle);
            }

            Ok(())
        })
        .plugin(
            tauri_plugin_autostart::Builder::new()
                .arg(AUTOSTART_ARG)
                .build(),
        )
        .plugin(tauri_plugin_global_shortcut::Builder::new().build())
        .plugin(tauri_plugin_process::init())
        .plugin(tauri_plugin_updater::Builder::new().build())
        .plugin(tauri_plugin_single_instance::init(|app, _argv, _cwd| {
            show_main_window(app);
        }))
        .on_window_event(|window, event| {
            if let WindowEvent::CloseRequested { api, .. } = event {
                if window.label() == MAIN_WINDOW_LABEL {
                    let settings_state = window.state::<AppSettingsState>();
                    let _ = persist_settings_state(&settings_state);
                }
                let flags = window.state::<RuntimeFlags>();
                if !flags.allow_exit.load(Ordering::Relaxed) {
                    let _ = window.hide();
                    api.prevent_close();
                }
            }

            if window.label() == MAIN_WINDOW_LABEL {
                if let WindowEvent::Moved(position) = event {
                    let settings_state = window.state::<AppSettingsState>();
                    let mut changed = false;

                    {
                        let mut settings = match settings_state.data.lock() {
                            Ok(value) => value,
                            Err(_) => return,
                        };

                        if settings.window.remember_position
                            && (settings.main_window_x != Some(position.x)
                                || settings.main_window_y != Some(position.y))
                        {
                            settings.main_window_x = Some(position.x);
                            settings.main_window_y = Some(position.y);
                            changed = true;
                        }
                    }

                    if changed {
                        let _ = persist_settings_state(&settings_state);
                    }
                }

                if let WindowEvent::Resized(size) = event {
                    if window.is_maximized().unwrap_or(false) || window.is_fullscreen().unwrap_or(false) {
                        return;
                    }

                    let settings_state = window.state::<AppSettingsState>();
                    let scale_factor = window.scale_factor().unwrap_or(1.0);
                    let logical_width = (size.width as f64 / scale_factor).round() as u32;
                    let logical_height = (size.height as f64 / scale_factor).round() as u32;
                    let mut changed = false;

                    {
                        let mut settings = match settings_state.data.lock() {
                            Ok(value) => value,
                            Err(_) => return,
                        };

                        if !settings.window.remember_main_window_size {
                            return;
                        }

                        let next_width = clamp_main_window_width(logical_width);
                        let next_height = clamp_main_window_height(logical_height);
                        if settings.main_window_width != Some(next_width)
                            || settings.main_window_height != Some(next_height)
                        {
                            settings.main_window_width = Some(next_width);
                            settings.main_window_height = Some(next_height);
                            changed = true;
                        }
                    }

                    if changed {
                        let _ = persist_settings_state(&settings_state);
                    }
                }

            }

            if window.label() == SELECTION_RESULT_WINDOW_LABEL {
                if let WindowEvent::Moved(position) = event {
                    let settings_state = window.state::<AppSettingsState>();
                    let mut changed = false;

                    {
                        let mut settings = match settings_state.data.lock() {
                            Ok(value) => value,
                            Err(_) => return,
                        };

                        if settings.selection_assistant.remember_result_window_position
                            && (settings.selection_result_window_x != Some(position.x)
                                || settings.selection_result_window_y != Some(position.y))
                        {
                            settings.selection_result_window_x = Some(position.x);
                            settings.selection_result_window_y = Some(position.y);
                            changed = true;
                        }
                    }

                    if changed {
                        let _ = persist_settings_state(&settings_state);
                    }
                }

                if let WindowEvent::Resized(size) = event {
                    if window.is_maximized().unwrap_or(false) || window.is_fullscreen().unwrap_or(false)
                    {
                        return;
                    }

                    let settings_state = window.state::<AppSettingsState>();
                    let scale_factor = window.scale_factor().unwrap_or(1.0);
                    let logical_width = (size.width as f64 / scale_factor).round() as u32;
                    let logical_height = (size.height as f64 / scale_factor).round() as u32;
                    let mut changed = false;

                    {
                        let mut settings = match settings_state.data.lock() {
                            Ok(value) => value,
                            Err(_) => return,
                        };

                        if !settings.window.remember_main_window_size {
                            return;
                        }

                        let next_width = clamp_selection_result_window_width(logical_width);
                        let next_height = clamp_selection_result_window_height(logical_height);
                        if settings.selection_result_window_width != Some(next_width)
                            || settings.selection_result_window_height != Some(next_height)
                        {
                            settings.selection_result_window_width = Some(next_width);
                            settings.selection_result_window_height = Some(next_height);
                            changed = true;
                        }
                    }

                    if changed {
                        let _ = persist_settings_state(&settings_state);
                    }
                }
            }

            if window.label() == OCR_RESULT_WINDOW_LABEL {
                if let WindowEvent::Moved(position) = event {
                    let settings_state = window.state::<AppSettingsState>();
                    let mut changed = false;

                    {
                        let mut settings = match settings_state.data.lock() {
                            Ok(value) => value,
                            Err(_) => return,
                        };

                        if settings.ocr.remember_result_window_position
                            && (settings.ocr_result_window_x != Some(position.x)
                                || settings.ocr_result_window_y != Some(position.y))
                        {
                            settings.ocr_result_window_x = Some(position.x);
                            settings.ocr_result_window_y = Some(position.y);
                            changed = true;
                        }
                    }

                    if changed {
                        let _ = persist_settings_state(&settings_state);
                    }
                }

                if let WindowEvent::Resized(size) = event {
                    if window.is_maximized().unwrap_or(false) || window.is_fullscreen().unwrap_or(false)
                    {
                        return;
                    }

                    let settings_state = window.state::<AppSettingsState>();
                    let scale_factor = window.scale_factor().unwrap_or(1.0);
                    let logical_width = (size.width as f64 / scale_factor).round() as u32;
                    let logical_height = (size.height as f64 / scale_factor).round() as u32;
                    let mut changed = false;

                    {
                        let mut settings = match settings_state.data.lock() {
                            Ok(value) => value,
                            Err(_) => return,
                        };

                        if !settings.window.remember_main_window_size {
                            return;
                        }

                        let next_width = clamp_ocr_result_window_width(logical_width);
                        let next_height = clamp_ocr_result_window_height(logical_height);
                        if settings.ocr_result_window_width != Some(next_width)
                            || settings.ocr_result_window_height != Some(next_height)
                        {
                            settings.ocr_result_window_width = Some(next_width);
                            settings.ocr_result_window_height = Some(next_height);
                            changed = true;
                        }
                    }

                    if changed {
                        let _ = persist_settings_state(&settings_state);
                    }
                }
            }

            if window.label() == MAIN_WINDOW_LABEL && matches!(event, WindowEvent::Focused(false))
            {
                let app_handle = window.app_handle().clone();
                std::thread::spawn(move || {
                    std::thread::sleep(Duration::from_millis(180));

                    let Some(main_window) = app_handle.get_webview_window(MAIN_WINDOW_LABEL) else {
                        return;
                    };

                    let settings_state = app_handle.state::<AppSettingsState>();
                    let _ = persist_settings_state(&settings_state);

                    let runtime_flags = app_handle.state::<RuntimeFlags>();
                    if runtime_flags.main_window_pinned.load(Ordering::Relaxed) {
                        return;
                    }
                    let suppress_until =
                        runtime_flags.suppress_auto_hide_until_ms.load(Ordering::Relaxed);
                    if suppress_until > now_epoch_millis() {
                        return;
                    }

                    let still_unfocused = main_window
                        .is_focused()
                        .map(|focused| !focused)
                        .unwrap_or(false);
                    if !still_unfocused {
                        return;
                    }

                    let is_pinned = main_window.is_always_on_top().unwrap_or(false);
                    if is_pinned {
                        return;
                    }

                    let should_hide = settings_state
                        .data
                        .lock()
                        .map(|settings| settings.window.auto_hide_on_blur)
                        .unwrap_or(true);

                    if should_hide {
                        let _ = main_window.hide();
                    }
                });
            }

            if window.label() == SELECTION_RESULT_WINDOW_LABEL
                && matches!(event, WindowEvent::Focused(false))
            {
                let app_handle = window.app_handle().clone();
                std::thread::spawn(move || {
                    std::thread::sleep(Duration::from_millis(150));
                    let Some(result_window) = app_handle.get_webview_window(SELECTION_RESULT_WINDOW_LABEL)
                    else {
                        return;
                    };
                    if result_window.is_always_on_top().unwrap_or(false) {
                        return;
                    }
                    let should_hide = app_handle
                        .try_state::<AppSettingsState>()
                        .and_then(|state| {
                            state
                                .data
                                .lock()
                                .ok()
                                .map(|settings| settings.window.auto_hide_on_blur)
                        })
                        .unwrap_or(true);
                    if !should_hide {
                        return;
                    }
                    let still_unfocused = result_window
                        .is_focused()
                        .map(|focused| !focused)
                        .unwrap_or(false);
                    if still_unfocused {
                        let _ = result_window.hide();
                    }
                });
            }

            if window.label() == OCR_CAPTURE_WINDOW_LABEL
                && matches!(event, WindowEvent::Focused(false))
            {
                let app_handle = window.app_handle().clone();
                std::thread::spawn(move || {
                    std::thread::sleep(Duration::from_millis(100));
                    let Some(capture_window) = app_handle.get_webview_window(OCR_CAPTURE_WINDOW_LABEL)
                    else {
                        return;
                    };
                    let mut still_active = false;
                    let mut suppress_until_ms = 0u64;
                    if let Some(ocr_runtime) = app_handle.try_state::<OcrRuntimeState>() {
                        still_active = ocr_runtime.capture_active.load(Ordering::Relaxed);
                        suppress_until_ms =
                            ocr_runtime.suppress_blur_until_ms.load(Ordering::Relaxed);
                    }
                    if !still_active {
                        return;
                    }
                    if now_epoch_millis() <= suppress_until_ms {
                        return;
                    }
                    let still_unfocused = capture_window
                        .is_focused()
                        .map(|focused| !focused)
                        .unwrap_or(true);
                    if !still_unfocused {
                        return;
                    }
                    if let Some(ocr_runtime) = app_handle.try_state::<OcrRuntimeState>() {
                        ocr_runtime.capture_active.store(false, Ordering::Relaxed);
                        ocr_runtime.suppress_blur_until_ms.store(0, Ordering::Relaxed);
                        if let Ok(mut snapshot) = ocr_runtime.capture_snapshot.lock() {
                            *snapshot = None;
                        }
                    }
                    hide_ocr_capture_window(&app_handle);
                    emit_ocr_capture_canceled(&app_handle);
                });
            }

            if window.label() == OCR_RESULT_WINDOW_LABEL
                && matches!(event, WindowEvent::Focused(false))
            {
                let app_handle = window.app_handle().clone();
                std::thread::spawn(move || {
                    std::thread::sleep(Duration::from_millis(150));
                    let Some(result_window) = app_handle.get_webview_window(OCR_RESULT_WINDOW_LABEL)
                    else {
                        return;
                    };
                    if result_window.is_always_on_top().unwrap_or(false) {
                        return;
                    }
                    let should_hide = app_handle
                        .try_state::<AppSettingsState>()
                        .and_then(|state| {
                            state
                                .data
                                .lock()
                                .ok()
                                .map(|settings| settings.window.auto_hide_on_blur)
                        })
                        .unwrap_or(true);
                    if !should_hide {
                        return;
                    }
                    let still_unfocused = result_window
                        .is_focused()
                        .map(|focused| !focused)
                        .unwrap_or(false);
                    if still_unfocused {
                        let _ = result_window.hide();
                    }
                });
            }
        })
        .invoke_handler(tauri::generate_handler![
            show_settings_window_cmd,
            show_main_window_cmd,
            hide_main_window_cmd,
            open_selection_bar,
            hide_selection_bar,
            copy_selection_text,
            open_search_with_text,
            run_selection_action,
            run_ocr_action_cmd,
            synthesize_tts_cmd,
            start_main_window_drag_cmd,
            set_main_window_pinned_cmd,
            get_main_window_pinned_cmd,
            set_result_window_pinned_cmd,
            get_result_window_pinned_cmd,
            minimize_selection_result_window,
            close_selection_result_window,
            start_ocr_capture_cmd,
            cancel_ocr_capture_cmd,
            complete_ocr_capture_cmd,
            set_ocr_result_window_pinned_cmd,
            get_ocr_result_window_pinned_cmd,
            minimize_ocr_result_window_cmd,
            close_ocr_result_window_cmd,
            get_settings,
            test_llm_api_cmd,
            test_ocr_vision_api_cmd,
            pick_history_storage_folder,
            update_settings,
            reset_settings,
            set_toggle_shortcut,
            set_toggle_ocr_shortcut,
            export_settings,
            import_settings,
            set_auto_hide_on_blur,
            get_auto_hide_on_blur,
            get_selection_assistant_status,
            set_selection_assistant_enabled,
            list_running_apps_cmd,
            sync_clipboard,
            get_history,
            set_last_opened_category_cmd,
            paste_entry_by_click,
            toggle_pin,
            toggle_favorite_text_cmd,
            remove_item,
            clear_history
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
