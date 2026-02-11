use serde::{Deserialize, Serialize};

pub const MAIN_WINDOW_LABEL: &str = "main";
pub const ACTION_BAR_WINDOW_LABEL: &str = "action-bar";
pub const TRANSLATE_WINDOW_LABEL: &str = "translate";
pub const SUMMARY_WINDOW_LABEL: &str = "summary";
pub const EXPLAIN_WINDOW_LABEL: &str = "explain";
pub const SETTINGS_WINDOW_LABEL: &str = "settings";

#[derive(Debug, Clone, Copy, Serialize, Deserialize, Eq, PartialEq)]
#[serde(rename_all = "kebab-case")]
pub enum WindowKind {
    Main,
    ActionBar,
    Translate,
    Summary,
    Explain,
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
            WindowKind::Settings => "设置",
        }
    }

    pub fn query_value(self) -> &'static str {
        self.label()
    }

    pub fn route(self) -> &'static str {
        match self {
            WindowKind::Main => "/",
            WindowKind::ActionBar => "/windows/action-bar",
            WindowKind::Translate => "/windows/translate",
            WindowKind::Summary => "/windows/summary",
            WindowKind::Explain => "/windows/explain",
            WindowKind::Settings => "/windows/settings",
        }
    }

    pub fn default_size(self) -> (f64, f64) {
        match self {
            WindowKind::Main => (1200.0, 780.0),
            WindowKind::ActionBar => (460.0, 52.0),
            WindowKind::Translate => (980.0, 720.0),
            WindowKind::Summary => (980.0, 720.0),
            WindowKind::Explain => (980.0, 720.0),
            WindowKind::Settings => (1040.0, 760.0),
        }
    }

    pub fn frameless(self) -> bool {
        !matches!(self, WindowKind::Main | WindowKind::Settings)
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
