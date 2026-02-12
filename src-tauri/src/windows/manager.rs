use tauri::{
    AppHandle, Manager, PhysicalPosition, Position, WebviewUrl, WebviewWindow, WebviewWindowBuilder,
};

use crate::windows::ids::WindowKind;

fn app_url_from_kind(kind: WindowKind) -> WebviewUrl {
    if kind == WindowKind::Main {
        WebviewUrl::App("index.html".into())
    } else {
        WebviewUrl::App(format!("index.html?window={}", kind.query_value()).into())
    }
}

fn init_script_for_kind(kind: WindowKind) -> String {
    format!("window.__SNAPPARSE_WINDOW_KIND = '{}';", kind.query_value())
}

pub fn ensure_window(app: &AppHandle, kind: WindowKind) -> tauri::Result<WebviewWindow> {
    if let Some(existing) = app.get_webview_window(kind.label()) {
        return Ok(existing);
    }

    let (width, height) = kind.default_size();
    let builder = WebviewWindowBuilder::new(app, kind.label(), app_url_from_kind(kind))
        .title(kind.title())
        .initialization_script(&init_script_for_kind(kind))
        .inner_size(width, height)
        .resizable(kind.resizable())
        .decorations(!kind.frameless())
        .transparent(kind.frameless())
        .always_on_top(kind.always_on_top())
        .skip_taskbar(kind.skip_taskbar())
        .visible(kind == WindowKind::Main || kind == WindowKind::Settings);

    builder.build()
}

pub fn show_window(app: &AppHandle, kind: WindowKind) -> tauri::Result<()> {
    let window = ensure_window(app, kind)?;
    window.show()?;

    if !matches!(kind, WindowKind::ActionBar) {
        window.set_focus()?;
    }

    Ok(())
}

pub fn hide_window(app: &AppHandle, kind: WindowKind) -> tauri::Result<()> {
    if let Some(window) = app.get_webview_window(kind.label()) {
        window.hide()?;
    }
    Ok(())
}

pub fn position_window(app: &AppHandle, kind: WindowKind, x: f64, y: f64) -> tauri::Result<()> {
    let window = ensure_window(app, kind)?;
    window.set_position(Position::Physical(PhysicalPosition::new(
        x.round() as i32,
        y.round() as i32,
    )))?;
    Ok(())
}
