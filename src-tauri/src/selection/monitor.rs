use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Mutex, OnceLock};
use std::time::{SystemTime, UNIX_EPOCH};

#[cfg(windows)]
use std::time::Duration;

use serde::Serialize;
use tauri::{AppHandle, Emitter, Manager};

use crate::selection::state::{selection_state, SelectionPoint};
use crate::windows::ids::WindowKind;
use crate::windows::manager;

const DRAG_TIME_THRESHOLD_MS: u128 = 300;
const DRAG_DISTANCE_THRESHOLD: f64 = 20.0;
const DOUBLE_CLICK_TIME_THRESHOLD_MS: u128 = 700;
const DOUBLE_CLICK_DISTANCE_THRESHOLD: f64 = 10.0;
const ACTION_BAR_DEFAULT_WIDTH: f64 = 402.0;
const ACTION_BAR_DEFAULT_HEIGHT: f64 = 62.0;
const ACTION_BAR_ABOVE_GAP: f64 = 14.0;
const ACTION_BAR_BELOW_GAP: f64 = 18.0;
const ACTION_BAR_MIN_PADDING: f64 = 10.0;

#[derive(Debug, Clone, Copy)]
pub struct ReleaseJudgement {
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
    press_x: i32,
    press_y: i32,
    previous_release_x: i32,
    previous_release_y: i32,
    current_x: i32,
    current_y: i32,
    pressed_time_ms: u128,
    previous_release_time_ms: u128,
    current_release_time_ms: u128,
) -> ReleaseJudgement {
    let drag_dx = f64::from(current_x - press_x);
    let drag_dy = f64::from(current_y - press_y);
    let drag_distance = (drag_dx * drag_dx + drag_dy * drag_dy).sqrt();

    let release_dx = f64::from(current_x - previous_release_x);
    let release_dy = f64::from(current_y - previous_release_y);
    let release_distance = (release_dx * release_dx + release_dy * release_dy).sqrt();

    let release_interval = current_release_time_ms.saturating_sub(previous_release_time_ms);

    let is_drag_selection =
        pressed_time_ms > DRAG_TIME_THRESHOLD_MS && drag_distance > DRAG_DISTANCE_THRESHOLD;
    let is_double_click_selection =
        release_interval < DOUBLE_CLICK_TIME_THRESHOLD_MS && release_distance < DOUBLE_CLICK_DISTANCE_THRESHOLD;

    ReleaseJudgement {
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
        MouseEvent::AbsoluteMove(_, _) => {}
        MouseEvent::RelativeMove(_, _) => {}
        MouseEvent::Press(MouseButton::Left) => {
            if let Some(point) = current_cursor_position(app) {
                if let Ok(mut state) = selection_state().lock() {
                    state.record_press(point);
                }
            }
        }
        MouseEvent::Release(MouseButton::Left) => {
            let app_handle = app.clone();
            std::thread::spawn(move || {
                handle_left_release(app_handle);
            });
        }
        _ => {}
    }
}

#[cfg(windows)]
fn handle_left_release(app: AppHandle) {
    let now_ms = now_epoch_ms();
    let point = match current_cursor_position(&app) {
        Some(value) => value,
        None => return,
    };

    let (point, release_snapshot) = {
        let mut state = match selection_state().lock() {
            Ok(guard) => guard,
            Err(_) => return,
        };

        let snapshot = state.begin_release(point, now_ms);
        (point, snapshot)
    };

    if !is_latest_release_id(release_snapshot.release_id) {
        return;
    }

    let judgement = judge_release(
        release_snapshot.press_position.x,
        release_snapshot.press_position.y,
        release_snapshot.previous_release_position.x,
        release_snapshot.previous_release_position.y,
        point.x,
        point.y,
        release_snapshot.pressed_duration_ms,
        release_snapshot.previous_release_time_ms,
        now_ms,
    );

    let hit_action_bar = point_hits_action_bar(&app, point);

    if hit_action_bar {
        return;
    }

    if point_hits_snapparse_windows(&app, point) {
        let _ = manager::hide_window(&app, WindowKind::ActionBar);
        return;
    }

    if !judgement.is_selection_candidate {
        let _ = manager::hide_window(&app, WindowKind::ActionBar);
        return;
    }

    if !is_latest_release_id(release_snapshot.release_id) {
        return;
    }

    let selected_text = match capture_selected_text_with_retry() {
        Some(value) => value,
        None => {
            let _ = manager::hide_window(&app, WindowKind::ActionBar);
            return;
        }
    };

    if !is_latest_release_id(release_snapshot.release_id) {
        return;
    }

    let payload = SelectedTextPayload {
        text: selected_text,
        source: "selection-monitor",
    };

    let (target_x, target_y) = compute_action_bar_position(&app, point);

    let _ = manager::position_window_physical(
        &app,
        WindowKind::ActionBar,
        target_x,
        target_y,
    );
    let _ = manager::show_window(&app, WindowKind::ActionBar);
    let _ = app.emit("selection-text-changed", payload);
}

fn compute_action_bar_position(app: &AppHandle, point: SelectionPoint) -> (f64, f64) {
    let (action_bar_width, action_bar_height) = action_bar_window_size(app);
    let mut x = f64::from(point.x) - action_bar_width / 2.0;
    let mut y = f64::from(point.y) - action_bar_height - ACTION_BAR_ABOVE_GAP;

    if let Ok(Some(monitor)) = app.monitor_from_point(f64::from(point.x), f64::from(point.y)) {
        let monitor_position = monitor.position();
        let monitor_size = monitor.size();

        let min_x = f64::from(monitor_position.x) + ACTION_BAR_MIN_PADDING;
        let max_x = f64::from(monitor_position.x) + f64::from(monitor_size.width)
            - action_bar_width
            - ACTION_BAR_MIN_PADDING;

        x = x.clamp(min_x, max_x.max(min_x));

        let top_limit = f64::from(monitor_position.y) + ACTION_BAR_MIN_PADDING;
        if y < top_limit {
            y = f64::from(point.y) + ACTION_BAR_BELOW_GAP;
        }

        let max_y = f64::from(monitor_position.y) + f64::from(monitor_size.height)
            - action_bar_height
            - ACTION_BAR_MIN_PADDING;
        if y > max_y {
            y = max_y;
        }
    } else {
        if x < ACTION_BAR_MIN_PADDING {
            x = ACTION_BAR_MIN_PADDING;
        }

        if y < ACTION_BAR_MIN_PADDING {
            y = f64::from(point.y) + ACTION_BAR_BELOW_GAP;
        }
    }

    (x, y)
}

fn action_bar_window_size(app: &AppHandle) -> (f64, f64) {
    let Some(window) = app.get_webview_window(WindowKind::ActionBar.label()) else {
        return (ACTION_BAR_DEFAULT_WIDTH, ACTION_BAR_DEFAULT_HEIGHT);
    };

    match window.outer_size() {
        Ok(size) => (f64::from(size.width), f64::from(size.height)),
        Err(_) => (ACTION_BAR_DEFAULT_WIDTH, ACTION_BAR_DEFAULT_HEIGHT),
    }
}

#[cfg(windows)]
fn is_latest_release_id(release_id: u64) -> bool {
    selection_state()
        .lock()
        .map(|state| state.is_latest_release(release_id))
        .unwrap_or(false)
}

#[cfg(windows)]
fn point_hits_action_bar(app: &AppHandle, point: SelectionPoint) -> bool {
    point_hits_window_label(app, WindowKind::ActionBar.label(), point)
}

#[cfg(windows)]
fn point_hits_snapparse_windows(app: &AppHandle, point: SelectionPoint) -> bool {
    const WINDOW_KINDS: [WindowKind; 7] = [
        WindowKind::Settings,
        WindowKind::Translate,
        WindowKind::Summary,
        WindowKind::Explain,
        WindowKind::Optimize,
        WindowKind::OcrCapture,
        WindowKind::Main,
    ];

    WINDOW_KINDS
        .iter()
        .copied()
        .any(|kind| point_hits_window_label(app, kind.label(), point))
}

#[cfg(windows)]
fn point_hits_window_label(app: &AppHandle, label: &str, point: SelectionPoint) -> bool {
    let Some(window) = app.get_webview_window(label) else {
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

    let left = f64::from(position.x);
    let top = f64::from(position.y);
    let right = left + f64::from(size.width);
    let bottom = top + f64::from(size.height);

    let x = f64::from(point.x);
    let y = f64::from(point.y);

    x >= left && x <= right && y >= top && y <= bottom
}

fn current_cursor_position(app: &AppHandle) -> Option<SelectionPoint> {
    let position = app.cursor_position().ok()?;
    Some(SelectionPoint {
        x: position.x.round() as i32,
        y: position.y.round() as i32,
    })
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

#[cfg(windows)]
fn capture_selected_text_with_retry() -> Option<String> {
    const RETRY_DELAYS_MS: [u64; 3] = [0, 40, 80];

    for delay in RETRY_DELAYS_MS {
        if delay > 0 {
            std::thread::sleep(Duration::from_millis(delay));
        }

        if let Some(text) = capture_selected_text() {
            return Some(text);
        }
    }

    None
}

fn now_epoch_ms() -> u128 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_millis())
        .unwrap_or_default()
}
