use tauri::{
    AppHandle, LogicalPosition, LogicalSize, Manager, Position, Size, WebviewUrl, WebviewWindow,
    WebviewWindowBuilder,
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
        eprintln!("[window] reusing existing window: {}", kind.label());
        return Ok(existing);
    }

    eprintln!("[window] creating new window: {} (frameless={}, transparent={}, decorations={})",
        kind.label(), kind.frameless(), kind.transparent(), !kind.frameless());

    let (width, height) = kind.default_size();
    let builder = WebviewWindowBuilder::new(app, kind.label(), app_url_from_kind(kind))
        .title(kind.title())
        .initialization_script(&init_script_for_kind(kind))
        .inner_size(width, height)
        .resizable(kind.resizable())
        .decorations(!kind.frameless())
        .transparent(kind.transparent())
        .always_on_top(kind.always_on_top())
        .skip_taskbar(kind.skip_taskbar())
        .visible(false);

    match builder.build() {
        Ok(window) => {
            eprintln!("[window] successfully created: {}", kind.label());
            Ok(window)
        }
        Err(error) => {
            eprintln!("[window] FAILED to create {}: {}", kind.label(), error);
            Err(error)
        }
    }
}

pub fn show_window(app: &AppHandle, kind: WindowKind) -> tauri::Result<()> {
    let window = ensure_window(app, kind)?;

    eprintln!("[window] showing: {}", kind.label());

    if let Err(e) = window.show() {
        eprintln!("[window] show() failed for {}: {}", kind.label(), e);
        return Err(e);
    }

    if !matches!(kind, WindowKind::ActionBar) {
        let _ = window.unminimize();
        if let Err(e) = window.set_focus() {
            eprintln!("[window] set_focus() failed for {}: {}", kind.label(), e);
        }
    }

    eprintln!("[window] show complete: {}", kind.label());
    Ok(())
}

pub fn hide_window(app: &AppHandle, kind: WindowKind) -> tauri::Result<()> {
    if let Some(window) = app.get_webview_window(kind.label()) {
        window.hide()?;
        eprintln!("[window] hidden: {}", kind.label());
    }
    Ok(())
}

pub fn position_window(app: &AppHandle, kind: WindowKind, x: f64, y: f64) -> tauri::Result<()> {
    let window = ensure_window(app, kind)?;
    eprintln!("[window] positioning {} to logical ({}, {})", kind.label(), x, y);
    window.set_position(Position::Logical(LogicalPosition::new(x, y)))?;
    Ok(())
}

pub fn resize_window(app: &AppHandle, kind: WindowKind, width: f64, height: f64) -> tauri::Result<()> {
    let window = ensure_window(app, kind)?;
    window.set_size(Size::Logical(LogicalSize::new(width.max(1.0), height.max(1.0))))?;
    Ok(())
}
