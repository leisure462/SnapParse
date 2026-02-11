use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Mutex, OnceLock};
use std::time::{SystemTime, UNIX_EPOCH};

use serde::Serialize;
use tauri::{AppHandle, Emitter, Manager};

use crate::selection::state::{selection_state, SelectionPoint};
use crate::windows::ids::{ACTION_BAR_WINDOW_LABEL, WindowKind};
use crate::windows::manager;

const DRAG_TIME_THRESHOLD_MS: u128 = 300;
const DRAG_DISTANCE_THRESHOLD: f64 = 20.0;
const DOUBLE_CLICK_TIME_THRESHOLD_MS: u128 = 700;
const DOUBLE_CLICK_DISTANCE_THRESHOLD: f64 = 10.0;
const ACTION_BAR_OFFSET_X: f64 = 7.0;
const ACTION_BAR_OFFSET_Y: f64 = 7.0;

#[derive(Debug, Clone, Copy)]
pub struct ReleaseJudgement {
    pub is_drag_selection: bool,
    pub is_double_click_selection: bool,
    pub is_selection_candidate: bool,
}

#[derive(Debug, thiserror::Error)]
pub enum SelectionMonitorError {
    #[error("mouse hook is already initialized")]
    AlreadyInitialized,
    #[error("failed to lock selection state")]
    LockPoisoned,
    #[error("failed to install mouse hook: {0}")]
    HookInstall(String),
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
struct SelectedTextPayload {
    text: String,
    source: &'static str,
}

pub fn judge_release(
    previous_x: i32,
    previous_y: i32,
    current_x: i32,
    current_y: i32,
    pressed_time_ms: u128,
    previous_release_time_ms: u128,
    current_release_time_ms: u128,
) -> ReleaseJudgement {
    let delta_x = f64::from(current_x - previous_x);
    let delta_y = f64::from(current_y - previous_y);
    let mouse_distance = (delta_x * delta_x + delta_y * delta_y).sqrt();

    let release_interval = current_release_time_ms.saturating_sub(previous_release_time_ms);

    let is_drag_selection =
        pressed_time_ms > DRAG_TIME_THRESHOLD_MS && mouse_distance > DRAG_DISTANCE_THRESHOLD;
    let is_double_click_selection =
        release_interval < DOUBLE_CLICK_TIME_THRESHOLD_MS && mouse_distance < DOUBLE_CLICK_DISTANCE_THRESHOLD;

    ReleaseJudgement {
        is_drag_selection,
        is_double_click_selection,
        is_selection_candidate: is_drag_selection || is_double_click_selection,
    }
}

#[cfg(windows)]
pub fn bind_mouse_hook(app: AppHandle) -> Result<(), SelectionMonitorError> {
    use mouce::MouseActions;

    static STARTED: AtomicBool = AtomicBool::new(false);
    if STARTED.swap(true, Ordering::SeqCst) {
        return Err(SelectionMonitorError::AlreadyInitialized);
    }

    let mut mouse = mouce::Mouse::new();
    let app_handle = app.clone();

    mouse
        .hook(Box::new(move |event| {
            handle_mouse_event(&app_handle, *event);
        }))
        .map_err(|error| SelectionMonitorError::HookInstall(error.to_string()))?;

    let storage = mouse_storage();
    let mut guard = storage
        .lock()
        .map_err(|_| SelectionMonitorError::LockPoisoned)?;
    *guard = Some(mouse);

    Ok(())
}

#[cfg(not(windows))]
pub fn bind_mouse_hook(_app: AppHandle) -> Result<(), SelectionMonitorError> {
    Ok(())
}

#[cfg(windows)]
fn mouse_storage() -> &'static Mutex<Option<mouce::Mouse>> {
    static STORE: OnceLock<Mutex<Option<mouce::Mouse>>> = OnceLock::new();
    STORE.get_or_init(|| Mutex::new(None))
}

#[cfg(windows)]
fn handle_mouse_event(app: &AppHandle, event: mouce::common::MouseEvent) {
    use mouce::common::{MouseButton, MouseEvent};

    match event {
        MouseEvent::AbsoluteMove(x, y) => {
            if let Ok(mut state) = selection_state().lock() {
                state.record_cursor_position(SelectionPoint { x, y });
            }
        }
        MouseEvent::Press(MouseButton::Left) => {
            if let Ok(mut state) = selection_state().lock() {
                state.record_press();
            }
        }
        MouseEvent::Release(MouseButton::Left) => {
            handle_left_release(app);
        }
        _ => {}
    }
}

#[cfg(windows)]
fn handle_left_release(app: &AppHandle) {
    let now_ms = now_epoch_ms();
    let (point, release_snapshot) = {
        let mut state = match selection_state().lock() {
            Ok(guard) => guard,
            Err(_) => return,
        };

        let point = state
            .last_cursor_position()
            .unwrap_or(SelectionPoint { x: 0, y: 0 });
        let snapshot = state.begin_release(point, now_ms);
        (point, snapshot)
    };

    let judgement = judge_release(
        release_snapshot.previous_release_position.x,
        release_snapshot.previous_release_position.y,
        point.x,
        point.y,
        release_snapshot.pressed_duration_ms,
        release_snapshot.previous_release_time_ms,
        now_ms,
    );

    let hit_action_bar = point_hits_action_bar(app, point);

    if !judgement.is_selection_candidate && !hit_action_bar {
        let _ = manager::hide_window(app, WindowKind::ActionBar);
        return;
    }

    if hit_action_bar {
        return;
    }

    let selected_text = match capture_selected_text() {
        Some(value) => value,
        None => {
            let _ = manager::hide_window(app, WindowKind::ActionBar);
            return;
        }
    };

    let payload = SelectedTextPayload {
        text: selected_text,
        source: "selection-monitor",
    };

    let _ = manager::position_window(
        app,
        WindowKind::ActionBar,
        f64::from(point.x) + ACTION_BAR_OFFSET_X,
        f64::from(point.y) + ACTION_BAR_OFFSET_Y,
    );
    let _ = manager::show_window(app, WindowKind::ActionBar);
    let _ = app.emit("selection-text-changed", payload);
}

#[cfg(windows)]
fn point_hits_action_bar(app: &AppHandle, point: SelectionPoint) -> bool {
    let Some(window) = app.get_webview_window(ACTION_BAR_WINDOW_LABEL) else {
        return false;
    };

    if !window.is_visible().unwrap_or(false) {
        return false;
    }

    let Ok(position) = window.outer_position() else {
        return false;
    };

    let Ok(size) = window.outer_size() else {
        return false;
    };

    let scale_factor = window.scale_factor().unwrap_or(1.0);
    let left = f64::from(position.x) / scale_factor;
    let top = f64::from(position.y) / scale_factor;
    let right = left + f64::from(size.width) / scale_factor;
    let bottom = top + f64::from(size.height) / scale_factor;

    let x = f64::from(point.x);
    let y = f64::from(point.y);

    x >= left && x <= right && y >= top && y <= bottom
}

#[cfg(windows)]
fn capture_selected_text() -> Option<String> {
    let text = get_selected_text::get_selected_text().ok()?;
    let trimmed = text.trim();

    if trimmed.is_empty() {
        None
    } else {
        Some(trimmed.to_owned())
    }
}

fn now_epoch_ms() -> u128 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_millis())
        .unwrap_or_default()
}
