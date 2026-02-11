use crate::selection::monitor::judge_release;

#[test]
fn drag_release_above_threshold_is_selection_candidate() {
    let result = judge_release(0, 0, 10, 10, 28, 0, 420, 900, 1700);
    assert!(result.is_selection_candidate);
    assert!(result.is_drag_selection);
}

#[test]
fn tiny_move_with_short_interval_is_double_click_selection() {
    let result = judge_release(104, 102, 100, 100, 104, 102, 120, 1000, 1500);
    assert!(result.is_selection_candidate);
    assert!(result.is_double_click_selection);
}

#[test]
fn short_press_without_distance_is_not_selection_candidate() {
    let result = judge_release(200, 200, 202, 201, 203, 202, 90, 1300, 2400);
    assert!(!result.is_selection_candidate);
}
