use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct AppSettings {
    #[serde(default)]
    pub general: GeneralSettings,
    pub api: ApiSettings,
    #[serde(default)]
    pub ocr: OcrSettings,
    pub toolbar: ToolbarSettings,
    pub window: WindowSettings,
    pub features: FeaturesSettings,
    pub advanced: AdvancedSettings,
}

impl Default for AppSettings {
    fn default() -> Self {
        Self {
            general: GeneralSettings::default(),
            api: ApiSettings::default(),
            ocr: OcrSettings::default(),
            toolbar: ToolbarSettings::default(),
            window: WindowSettings::default(),
            features: FeaturesSettings::default(),
            advanced: AdvancedSettings::default(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct GeneralSettings {
    #[serde(default)]
    pub launch_at_startup: bool,
    #[serde(default)]
    pub silent_startup: bool,
    #[serde(default)]
    pub language: AppLanguage,
}

impl Default for GeneralSettings {
    fn default() -> Self {
        Self {
            launch_at_startup: false,
            silent_startup: false,
            language: AppLanguage::default(),
        }
    }
}

#[derive(Debug, Copy, Clone, Serialize, Deserialize, Eq, PartialEq)]
pub enum AppLanguage {
    #[serde(rename = "zh-CN")]
    ZhCn,
    #[serde(rename = "en-US")]
    EnUs,
}

impl Default for AppLanguage {
    fn default() -> Self {
        AppLanguage::ZhCn
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct ApiSettings {
    pub base_url: String,
    pub api_key: String,
    pub model: String,
    pub timeout_ms: u64,
    pub temperature: f32,
    #[serde(default)]
    pub feature_models: FeatureModels,
}

impl Default for ApiSettings {
    fn default() -> Self {
        let model = String::from("gpt-4o-mini");

        Self {
            base_url: String::from("https://api.openai.com/v1"),
            api_key: String::new(),
            model: model.clone(),
            timeout_ms: 30_000,
            temperature: 0.3,
            feature_models: FeatureModels {
                translate: model.clone(),
                summarize: model.clone(),
                explain: model,
                optimize: String::from("gpt-4o-mini"),
            },
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct OcrSettings {
    #[serde(default)]
    pub enabled: bool,
    #[serde(default = "default_ocr_capture_hotkey")]
    pub capture_hotkey: String,
    #[serde(default = "default_ocr_quick_hotkey")]
    pub quick_ocr_hotkey: String,
    #[serde(default)]
    pub capture_default_mode: CaptureMode,
    #[serde(default = "default_ocr_show_shortcut_hints")]
    pub show_shortcut_hints: bool,
    #[serde(default)]
    pub mode_hotkeys: OcrModeHotkeys,
    #[serde(default)]
    pub provider: OcrProvider,
    #[serde(default = "default_ocr_base_url")]
    pub base_url: String,
    #[serde(default)]
    pub api_key: String,
    #[serde(default = "default_ocr_model")]
    pub model: String,
    #[serde(default = "default_ocr_timeout_ms")]
    pub timeout_ms: u64,
    #[serde(default = "default_ocr_prompt")]
    pub prompt: String,
    #[serde(default = "default_ocr_post_action_id")]
    pub post_action_id: String,
}

impl Default for OcrSettings {
    fn default() -> Self {
        Self {
            enabled: false,
            capture_hotkey: default_ocr_capture_hotkey(),
            quick_ocr_hotkey: default_ocr_quick_hotkey(),
            capture_default_mode: CaptureMode::default(),
            show_shortcut_hints: default_ocr_show_shortcut_hints(),
            mode_hotkeys: OcrModeHotkeys::default(),
            provider: OcrProvider::default(),
            base_url: default_ocr_base_url(),
            api_key: String::new(),
            model: default_ocr_model(),
            timeout_ms: default_ocr_timeout_ms(),
            prompt: default_ocr_prompt(),
            post_action_id: default_ocr_post_action_id(),
        }
    }
}

fn default_ocr_capture_hotkey() -> String {
    String::from("Ctrl+Shift+X")
}

fn default_ocr_quick_hotkey() -> String {
    String::from("Ctrl+Shift+O")
}

fn default_ocr_show_shortcut_hints() -> bool {
    true
}

fn default_ocr_base_url() -> String {
    String::from("https://api.openai.com/v1")
}

fn default_ocr_model() -> String {
    String::from("gpt-4o-mini")
}

fn default_ocr_timeout_ms() -> u64 {
    45_000
}

fn default_ocr_prompt() -> String {
    String::from("You are an OCR engine. Extract all visible text from the image in natural reading order. Return plain text only, preserve line breaks when meaningful, and do not add explanations.")
}

fn default_ocr_post_action_id() -> String {
    String::from("translate")
}

#[derive(Debug, Copy, Clone, Serialize, Deserialize, Eq, PartialEq)]
#[serde(rename_all = "kebab-case")]
pub enum OcrProvider {
    OpenaiVision,
    GlmOcr,
}

impl Default for OcrProvider {
    fn default() -> Self {
        OcrProvider::OpenaiVision
    }
}

#[derive(Debug, Copy, Clone, Serialize, Deserialize, Eq, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum CaptureMode {
    Region,
    Fullscreen,
    Window,
}

impl Default for CaptureMode {
    fn default() -> Self {
        CaptureMode::Region
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct OcrModeHotkeys {
    #[serde(default = "default_mode_hotkey_region")]
    pub region: String,
    #[serde(default = "default_mode_hotkey_fullscreen")]
    pub fullscreen: String,
    #[serde(default = "default_mode_hotkey_window")]
    pub window: String,
}

impl Default for OcrModeHotkeys {
    fn default() -> Self {
        Self {
            region: default_mode_hotkey_region(),
            fullscreen: default_mode_hotkey_fullscreen(),
            window: default_mode_hotkey_window(),
        }
    }
}

fn default_mode_hotkey_region() -> String {
    String::from("Ctrl+Shift+X")
}

fn default_mode_hotkey_fullscreen() -> String {
    String::from("Ctrl+Shift+A")
}

fn default_mode_hotkey_window() -> String {
    String::from("Ctrl+Shift+M")
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct FeatureModels {
    #[serde(default = "default_feature_model")]
    pub translate: String,
    #[serde(default = "default_feature_model")]
    pub summarize: String,
    #[serde(default = "default_feature_model")]
    pub explain: String,
    #[serde(default = "default_feature_model")]
    pub optimize: String,
}

impl Default for FeatureModels {
    fn default() -> Self {
        Self {
            translate: default_feature_model(),
            summarize: default_feature_model(),
            explain: default_feature_model(),
            optimize: default_feature_model(),
        }
    }
}

fn default_feature_model() -> String {
    String::from("gpt-4o-mini")
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct ToolbarSettings {
    pub trigger_mode: TriggerMode,
    #[serde(default = "default_trigger_hotkey")]
    pub trigger_hotkey: String,
    pub compact_mode: bool,
    pub show_label: bool,
    pub theme_mode: ThemeMode,
    pub actions: Vec<ToolbarAction>,
}

fn default_trigger_hotkey() -> String {
    String::from("Ctrl+Shift+Space")
}

impl Default for ToolbarSettings {
    fn default() -> Self {
        Self {
            trigger_mode: TriggerMode::Selection,
            trigger_hotkey: default_trigger_hotkey(),
            compact_mode: false,
            show_label: true,
            theme_mode: ThemeMode::Dark,
            actions: vec![
                ToolbarAction::new(ActionId::Translate, "翻译", 0),
                ToolbarAction::new(ActionId::Explain, "解释", 1),
                ToolbarAction::new(ActionId::Summarize, "总结", 2),
                ToolbarAction::new(ActionId::Optimize, "优化", 3),
                ToolbarAction::new(ActionId::Search, "搜索", 4),
                ToolbarAction::new(ActionId::Copy, "复制", 5),
            ],
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct ToolbarAction {
    pub id: ActionId,
    pub label: String,
    pub enabled: bool,
    pub order: u8,
}

impl ToolbarAction {
    fn new(id: ActionId, label: &str, order: u8) -> Self {
        Self {
            id,
            label: String::from(label),
            enabled: true,
            order,
        }
    }
}

#[derive(Debug, Copy, Clone, Serialize, Deserialize, Eq, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum ThemeMode {
    Light,
    Dark,
    System,
}

#[derive(Debug, Copy, Clone, Serialize, Deserialize, Eq, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum TriggerMode {
    Selection,
    Ctrl,
    Hotkey,
}

#[derive(Debug, Copy, Clone, Serialize, Deserialize, Eq, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum ActionId {
    Translate,
    Explain,
    Summarize,
    Optimize,
    Search,
    Copy,
}

#[derive(Debug, Copy, Clone, Serialize, Deserialize, Eq, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum WindowSizePreset {
    Large,
    Medium,
    Small,
}

impl Default for WindowSizePreset {
    fn default() -> Self {
        WindowSizePreset::Large
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct WindowSettings {
    pub follow_toolbar: bool,
    pub remember_size: bool,
    pub auto_close: bool,
    pub auto_pin: bool,
    #[serde(default)]
    pub window_size: WindowSizePreset,
    #[serde(default = "default_font_size")]
    pub font_size: u32,
}

fn default_font_size() -> u32 {
    14
}

impl Default for WindowSettings {
    fn default() -> Self {
        Self {
            follow_toolbar: true,
            remember_size: true,
            auto_close: false,
            auto_pin: false,
            window_size: WindowSizePreset::default(),
            font_size: default_font_size(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct FeaturesSettings {
    #[serde(default)]
    pub custom_actions_enabled: bool,
    #[serde(default = "default_enabled_actions")]
    pub enabled_actions: Vec<ActionId>,
    #[serde(default)]
    pub custom_actions: Vec<CustomFeatureAction>,
}

impl Default for FeaturesSettings {
    fn default() -> Self {
        Self {
            custom_actions_enabled: false,
            enabled_actions: default_enabled_actions(),
            custom_actions: Vec::new(),
        }
    }
}

fn default_enabled_actions() -> Vec<ActionId> {
    vec![
        ActionId::Translate,
        ActionId::Explain,
        ActionId::Summarize,
        ActionId::Optimize,
        ActionId::Search,
        ActionId::Copy,
    ]
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct CustomFeatureAction {
    pub id: String,
    pub name: String,
    pub icon: String,
    pub prompt: String,
    #[serde(default)]
    pub model: String,
    pub enabled: bool,
    pub order: u16,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct AdvancedSettings {
    pub app_filter_mode: AppFilterMode,
    pub app_list: Vec<String>,
    pub log_level: LogLevel,
}

impl Default for AdvancedSettings {
    fn default() -> Self {
        Self {
            app_filter_mode: AppFilterMode::Off,
            app_list: Vec::new(),
            log_level: LogLevel::Info,
        }
    }
}

#[derive(Debug, Copy, Clone, Serialize, Deserialize, Eq, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum AppFilterMode {
    Off,
    Whitelist,
    Blacklist,
}

#[derive(Debug, Copy, Clone, Serialize, Deserialize, Eq, PartialEq)]
#[serde(rename_all = "lowercase")]
pub enum LogLevel {
    Error,
    Warn,
    Info,
    Debug,
}
