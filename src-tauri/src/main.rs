#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod commands;
mod ai;
mod selection;
mod settings;
mod windows;

use tauri::menu::MenuBuilder;
use tauri::tray::{MouseButton, MouseButtonState, TrayIconBuilder, TrayIconEvent};
use tauri::Manager;
use single_instance::SingleInstance;

pub const APP_NAME: &str = "SnapParse";

#[cfg(test)]
#[path = "tests/mod.rs"]
mod test_suite;

fn main() {
    let single_instance = SingleInstance::new("com.leisure462.snapparse.single-instance")
        .expect("failed to create single instance lock");
    if !single_instance.is_single() {
        return;
    }

    tauri::Builder::default()
        .setup(|app| {
            if let Some(main_window) = app.get_webview_window("main") {
                let _ = main_window.hide();
            }

            if let Err(error) = selection::monitor::bind_mouse_hook(app.handle().clone()) {
                eprintln!("selection monitor is not active: {error}");
            }

            if let Err(error) = setup_tray(app) {
                eprintln!("tray setup failed: {error}");
            }

            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            commands::settings::get_settings,
            commands::settings::save_settings,
            commands::settings::reset_settings,
            commands::ai::process_selected_text,
            commands::ai::test_api_connection,
            commands::windows::open_window,
            commands::windows::close_window,
            commands::windows::move_window,
            commands::windows::resize_window
        ])
        .run(tauri::generate_context!())
        .expect("error while running SnapParse application");
}

fn setup_tray(app: &tauri::App) -> tauri::Result<()> {
    let tray_menu = MenuBuilder::new(app)
        .text("open-settings", "打开设置")
        .separator()
        .text("quit", "退出")
        .build()?;

    let mut tray_builder = TrayIconBuilder::with_id("snapparse-tray")
        .menu(&tray_menu)
        .show_menu_on_left_click(false)
        .tooltip("SnapParse")
        .on_menu_event(|app_handle, event| {
            let event_id = event.id.as_ref();

            if event_id == "open-settings" || event_id.ends_with("open-settings") {
                let _ = windows::manager::show_window(app_handle, windows::ids::WindowKind::Settings);
                return;
            }

            if event_id == "quit" || event_id.ends_with("quit") {
                app_handle.exit(0);
                std::process::exit(0);
                return;
            }
        })
        .on_tray_icon_event(|tray, event| {
            if let TrayIconEvent::Click {
                button: MouseButton::Left,
                button_state: MouseButtonState::Up,
                ..
            } = event
            {
                let app_handle = tray.app_handle();
                let _ = windows::manager::show_window(&app_handle, windows::ids::WindowKind::Settings);
            }
        });

    if let Some(icon) = app.default_window_icon() {
        tray_builder = tray_builder.icon(icon.clone());
    }

    tray_builder.build(app)?;

    Ok(())
}

#[cfg(test)]
mod tests {
    #[test]
    fn app_name_constant_exists() {
        assert_eq!("SnapParse", crate::APP_NAME);
    }
}
