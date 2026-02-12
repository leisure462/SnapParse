use std::sync::{Mutex, OnceLock};
use std::time::Instant;

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct SelectionPoint {
    pub x: i32,
    pub y: i32,
}

#[derive(Debug, Clone)]
pub struct ReleaseSnapshot {
    pub release_id: u64,
    pub press_position: SelectionPoint,
    pub previous_release_time_ms: u128,
    pub previous_release_position: SelectionPoint,
    pub pressed_duration_ms: u128,
}

#[derive(Debug)]
pub struct SelectionRuntimeState {
    previous_press_instant: Option<Instant>,
    previous_press_position: Option<SelectionPoint>,
    previous_release_time_ms: Option<u128>,
    previous_release_position: Option<SelectionPoint>,
    release_thread_id: u64,
}

impl Default for SelectionRuntimeState {
    fn default() -> Self {
        Self {
            previous_press_instant: None,
            previous_press_position: None,
            previous_release_time_ms: None,
            previous_release_position: None,
            release_thread_id: 0,
        }
    }
}

impl SelectionRuntimeState {
    pub fn record_press(&mut self, point: SelectionPoint) {
        self.previous_press_instant = Some(Instant::now());
        self.previous_press_position = Some(point);
    }

    pub fn begin_release(&mut self, current: SelectionPoint, now_ms: u128) -> ReleaseSnapshot {
        self.release_thread_id = self.release_thread_id.wrapping_add(1);

        let press_position = self.previous_press_position.unwrap_or(current);

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
            release_id: self.release_thread_id,
            press_position,
            previous_release_time_ms,
            previous_release_position,
            pressed_duration_ms,
        }
    }

    pub fn is_latest_release(&self, release_id: u64) -> bool {
        self.release_thread_id == release_id
    }

}

pub fn selection_state() -> &'static Mutex<SelectionRuntimeState> {
    static STATE: OnceLock<Mutex<SelectionRuntimeState>> = OnceLock::new();
    STATE.get_or_init(|| Mutex::new(SelectionRuntimeState::default()))
}
