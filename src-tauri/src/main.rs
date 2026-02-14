#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod commands;
mod ai;
mod ocr;
mod selection;
mod settings;
mod windows;

use tauri::menu::MenuBuilder;
use tauri::tray::{MouseButton, MouseButtonState, TrayIcon, TrayIconBuilder, TrayIconEvent};
use tauri::Manager;
use tauri_plugin_autostart::{MacosLauncher, ManagerExt};
use single_instance::SingleInstance;

pub const APP_NAME: &str = "SnapParse";

#[cfg(test)]
#[path = "tests/mod.rs"]
mod test_suite;

fn is_autostart_launch() -> bool {
    std::env::args().any(|arg| arg == "--autostart")
}

fn load_startup_settings(app: &tauri::AppHandle) -> settings::model::AppSettings {
    let config_root = match app.path().app_config_dir() {
        Ok(path) => path,
        Err(error) => {
            eprintln!("failed to resolve app config dir: {error}");
            return settings::model::AppSettings::default();
        }
    };

    settings::store::load_settings(&config_root).unwrap_or_else(|error| {
        eprintln!("failed to load settings at startup: {error}");
        settings::model::AppSettings::default()
    })
}

fn sync_autostart_at_startup(app: &tauri::AppHandle, enabled: bool) {
    let autolaunch = app.autolaunch();
    match autolaunch.is_enabled() {
        Ok(current) if current == enabled => {}
        Ok(_) if enabled => {
            if let Err(error) = autolaunch.enable() {
                eprintln!("failed to enable auto-launch: {error}");
            }
        }
        Ok(_) => {
            if let Err(error) = autolaunch.disable() {
                eprintln!("failed to disable auto-launch: {error}");
            }
        }
        Err(error) => {
            eprintln!("failed to query auto-launch state: {error}");
        }
    }
}

fn main() {
    let single_instance = SingleInstance::new("com.leisure462.snapparse.single-instance")
        .expect("failed to create single instance lock");
    if !single_instance.is_single() {
        return;
    }

    tauri::Builder::default()
        .plugin(tauri_plugin_autostart::init(
            MacosLauncher::LaunchAgent,
            Some(vec!["--autostart"]),
        ))
        .plugin(tauri_plugin_global_shortcut::Builder::new().build())
        .setup(|app| {
            if let Some(main_window) = app.get_webview_window("main") {
                let _ = main_window.hide();
            }

            // Pre-create all windows (hidden) at startup so they are ready
            // when the user clicks an action button. This avoids blocking
            // the main thread with heavy webview creation at runtime.
            windows::manager::precreate_all_windows(app.handle());

            if let Err(error) = selection::monitor::bind_mouse_hook(app.handle().clone()) {
                eprintln!("selection monitor is not active: {error}");
            }

            if let Err(error) = setup_tray(app) {
                eprintln!("tray setup failed: {error}");
            }

            let settings = load_startup_settings(&app.handle());
            sync_autostart_at_startup(&app.handle(), settings.general.launch_at_startup);
            if let Err(error) = ocr::sync_ocr_hotkey(&app.handle(), &settings) {
                eprintln!("failed to sync OCR hotkey: {error}");
            }

            if !is_autostart_launch() && !settings.general.silent_startup {
                let _ = windows::manager::show_window(&app.handle(), windows::ids::WindowKind::Settings);
            }

            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            commands::settings::get_settings,
            commands::settings::save_settings,
            commands::settings::reset_settings,
            commands::ai::process_selected_text,
            commands::ai::stream_process_text,
            commands::ai::test_api_connection,
            commands::ocr::start_ocr_capture,
            commands::ocr::run_ocr_capture,
            commands::ocr::capture_screenshot_preview,
            commands::ocr::resolve_window_capture_hint,
            commands::windows::open_window,
            commands::windows::close_window,
            commands::windows::move_window,
            commands::windows::resize_window,
            commands::windows::open_external_url,
            commands::windows::set_pending_optimize_request,
            commands::windows::take_pending_optimize_request
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
            }
        })
        .on_tray_icon_event(|tray, event| {
            if let TrayIconEvent::Click {
                button,
                button_state,
                ..
            } = event
            {
                if matches!(button, MouseButton::Left)
                    && matches!(button_state, MouseButtonState::Up)
                {
                    let app_handle = tray.app_handle();
                    let _ = windows::manager::show_window(&app_handle, windows::ids::WindowKind::Settings);
                }
            }
        });

    if let Some(icon) = app.default_window_icon() {
        tray_builder = tray_builder.icon(icon.clone());
    }

    let tray_icon: TrayIcon = tray_builder.build(app)?;
    app.manage(tray_icon);

    Ok(())
}

#[cfg(test)]
mod tests {
    #[test]
    fn app_name_constant_exists() {
        assert_eq!("SnapParse", crate::APP_NAME);
    }
}
