use std::collections::HashSet;

use crate::windows::ids::WindowKind;

#[test]
fn summary_query_value_is_correct() {
    assert_eq!(WindowKind::Summary.query_value(), "summary");
}

#[test]
fn window_labels_are_unique() {
    let labels = [
        WindowKind::Main.label(),
        WindowKind::ActionBar.label(),
        WindowKind::Translate.label(),
        WindowKind::Summary.label(),
        WindowKind::Explain.label(),
        WindowKind::Settings.label(),
    ];

    let unique: HashSet<_> = labels.into_iter().collect();
    assert_eq!(unique.len(), labels.len());
}

#[test]
fn action_bar_window_is_always_on_top_and_non_resizable() {
    assert!(WindowKind::ActionBar.always_on_top());
    assert!(!WindowKind::ActionBar.resizable());
}
