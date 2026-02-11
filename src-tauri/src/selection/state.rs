use std::sync::{Mutex, OnceLock};
use std::time::Instant;

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct SelectionPoint {
    pub x: i32,
    pub y: i32,
}

#[derive(Debug, Clone)]
pub struct ReleaseSnapshot {
    pub previous_release_time_ms: u128,
    pub previous_release_position: SelectionPoint,
    pub pressed_duration_ms: u128,
}

#[derive(Debug)]
pub struct SelectionRuntimeState {
    previous_press_instant: Option<Instant>,
    previous_release_time_ms: Option<u128>,
    previous_release_position: Option<SelectionPoint>,
    last_cursor_position: Option<SelectionPoint>,
    release_thread_id: u64,
}

impl Default for SelectionRuntimeState {
    fn default() -> Self {
        Self {
            previous_press_instant: None,
            previous_release_time_ms: None,
            previous_release_position: None,
            last_cursor_position: None,
            release_thread_id: 0,
        }
    }
}

impl SelectionRuntimeState {
    pub fn record_press(&mut self) {
        self.previous_press_instant = Some(Instant::now());
    }

    pub fn record_cursor_position(&mut self, point: SelectionPoint) {
        self.last_cursor_position = Some(point);
    }

    pub fn last_cursor_position(&self) -> Option<SelectionPoint> {
        self.last_cursor_position
    }

    pub fn begin_release(&mut self, current: SelectionPoint, now_ms: u128) -> ReleaseSnapshot {
        self.release_thread_id = self.release_thread_id.wrapping_add(1);

        let previous_release_position = self.previous_release_position.unwrap_or(current);
        let previous_release_time_ms = self
            .previous_release_time_ms
            .unwrap_or_else(|| now_ms.saturating_sub(10_000));

        let pressed_duration_ms = self
            .previous_press_instant
            .map(|instant| instant.elapsed().as_millis())
            .unwrap_or_default();

        self.previous_release_position = Some(current);
        self.previous_release_time_ms = Some(now_ms);

        ReleaseSnapshot {
            previous_release_time_ms,
            previous_release_position,
            pressed_duration_ms,
        }
    }

}

pub fn selection_state() -> &'static Mutex<SelectionRuntimeState> {
    static STATE: OnceLock<Mutex<SelectionRuntimeState>> = OnceLock::new();
    STATE.get_or_init(|| Mutex::new(SelectionRuntimeState::default()))
}
