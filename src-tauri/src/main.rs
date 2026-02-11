#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod commands;
mod ai;
mod selection;
mod settings;
mod windows;

pub const APP_NAME: &str = "SnapParse";

#[cfg(test)]
#[path = "tests/mod.rs"]
mod test_suite;

fn main() {
    tauri::Builder::default()
        .setup(|app| {
            if let Err(error) = selection::monitor::bind_mouse_hook(app.handle().clone()) {
                eprintln!("selection monitor is not active: {error}");
            }

            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            commands::settings::get_settings,
            commands::settings::save_settings,
            commands::settings::reset_settings,
            commands::ai::process_selected_text,
            commands::windows::open_window,
            commands::windows::close_window,
            commands::windows::move_window
        ])
        .run(tauri::generate_context!())
        .expect("error while running SnapParse application");
}

#[cfg(test)]
mod tests {
    #[test]
    fn app_name_constant_exists() {
        assert_eq!("SnapParse", crate::APP_NAME);
    }
}
