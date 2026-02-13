use serde::{Deserialize, Serialize};

pub const MAIN_WINDOW_LABEL: &str = "main";
pub const ACTION_BAR_WINDOW_LABEL: &str = "action-bar";
pub const TRANSLATE_WINDOW_LABEL: &str = "translate";
pub const SUMMARY_WINDOW_LABEL: &str = "summary";
pub const EXPLAIN_WINDOW_LABEL: &str = "explain";
pub const OPTIMIZE_WINDOW_LABEL: &str = "optimize";
pub const SETTINGS_WINDOW_LABEL: &str = "settings";

#[derive(Debug, Clone, Copy, Serialize, Deserialize, Eq, PartialEq)]
#[serde(rename_all = "kebab-case")]
pub enum WindowKind {
    Main,
    ActionBar,
    Translate,
    Summary,
    Explain,
    Optimize,
    Settings,
}

impl WindowKind {
    pub fn label(self) -> &'static str {
        match self {
            WindowKind::Main => MAIN_WINDOW_LABEL,
            WindowKind::ActionBar => ACTION_BAR_WINDOW_LABEL,
            WindowKind::Translate => TRANSLATE_WINDOW_LABEL,
            WindowKind::Summary => SUMMARY_WINDOW_LABEL,
            WindowKind::Explain => EXPLAIN_WINDOW_LABEL,
            WindowKind::Optimize => OPTIMIZE_WINDOW_LABEL,
            WindowKind::Settings => SETTINGS_WINDOW_LABEL,
        }
    }

    pub fn title(self) -> &'static str {
        match self {
            WindowKind::Main => "SnapParse",
            WindowKind::ActionBar => "SnapParse Action Bar",
            WindowKind::Translate => "翻译",
            WindowKind::Summary => "总结",
            WindowKind::Explain => "解释",
            WindowKind::Optimize => "优化",
            WindowKind::Settings => "设置",
        }
    }

    pub fn query_value(self) -> &'static str {
        self.label()
    }

    pub fn default_size(self) -> (f64, f64) {
        match self {
            WindowKind::Main => (1200.0, 780.0),
            WindowKind::ActionBar => (402.0, 48.0),
            // Feature windows default to "large" preset; actual size set via resize_window at runtime
            WindowKind::Translate => (680.0, 520.0),
            WindowKind::Summary => (680.0, 520.0),
            WindowKind::Explain => (680.0, 520.0),
            WindowKind::Optimize => (680.0, 520.0),
            WindowKind::Settings => (800.0, 600.0),
        }
    }

    pub fn frameless(self) -> bool {
        !matches!(self, WindowKind::Main | WindowKind::Settings)
    }

    /// Only action bar uses transparent webview so the capsule can float.
    /// Feature windows stay opaque for readability and rendering stability.
    pub fn transparent(self) -> bool {
        matches!(self, WindowKind::ActionBar)
    }

    pub fn resizable(self) -> bool {
        !matches!(self, WindowKind::ActionBar)
    }

    pub fn always_on_top(self) -> bool {
        matches!(self, WindowKind::ActionBar)
    }

    pub fn skip_taskbar(self) -> bool {
        matches!(self, WindowKind::ActionBar)
    }
}
