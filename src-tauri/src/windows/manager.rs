use tauri::{
    AppHandle, LogicalPosition, LogicalSize, Manager, PhysicalPosition, Position, Size,
    WebviewUrl, WebviewWindow, WebviewWindowBuilder,
};

use crate::windows::ids::WindowKind;

/// All window kinds that should be pre-created at startup.
const PRECREATE_KINDS: [WindowKind; 5] = [
    WindowKind::ActionBar,
    WindowKind::Translate,
    WindowKind::Summary,
    WindowKind::Explain,
    WindowKind::Settings,
];

fn app_url_from_kind(kind: WindowKind) -> WebviewUrl {
    if kind == WindowKind::Main {
        WebviewUrl::App("index.html".into())
    } else {
        WebviewUrl::App(format!("index.html?window={}", kind.query_value()).into())
    }
}

fn init_script_for_kind(kind: WindowKind) -> String {
    format!(
        "window.__SNAPPARSE_WINDOW_KIND = '{}';",
        kind.query_value()
    )
}

/// Pre-create all dynamic windows (hidden) at startup so they are ready
/// when the user clicks an action. This avoids heavy webview creation
/// at runtime which can block the main thread and cause freezes.
pub fn precreate_all_windows(app: &AppHandle) {
    for kind in PRECREATE_KINDS {
        match ensure_window(app, kind) {
            Ok(_) => eprintln!("[startup] pre-created window: {}", kind.label()),
            Err(e) => eprintln!("[startup] FAILED to pre-create {}: {}", kind.label(), e),
        }
    }
}

pub fn ensure_window(app: &AppHandle, kind: WindowKind) -> tauri::Result<WebviewWindow> {
    if let Some(existing) = app.get_webview_window(kind.label()) {
        return Ok(existing);
    }

    eprintln!(
        "[window] creating: {} (decorations={}, transparent={})",
        kind.label(),
        !kind.frameless(),
        kind.transparent()
    );

    let (width, height) = kind.default_size();
    let mut builder = WebviewWindowBuilder::new(app, kind.label(), app_url_from_kind(kind))
        .title(kind.title())
        .initialization_script(&init_script_for_kind(kind))
        .inner_size(width, height)
        .resizable(kind.resizable())
        .decorations(!kind.frameless())
        .transparent(kind.transparent())
        .always_on_top(kind.always_on_top())
        .skip_taskbar(kind.skip_taskbar())
        .visible(false);

    // For transparent windows (action bar), set the WebView2 default background
    // color to fully transparent RGBA(0,0,0,0). On Windows, `.transparent(true)`
    // alone only makes the Win32 window layered – WebView2 keeps its own opaque
    // background unless we explicitly clear it via `background_color`.
    if kind.transparent() {
        builder = builder.background_color(tauri::Color(0, 0, 0, 0));

        // Also inject an early init script that forces document background to
        // transparent before CSS loads.
        builder = builder.initialization_script(
            "document.documentElement.style.background='transparent';\
             document.body && (document.body.style.background='transparent');"
        );
    }

    let window = builder.build()?;

    eprintln!("[window] created OK: {}", kind.label());
    Ok(window)
}

pub fn show_window(app: &AppHandle, kind: WindowKind) -> tauri::Result<()> {
    let window = ensure_window(app, kind)?;

    window.show()?;

    if !matches!(kind, WindowKind::ActionBar) {
        let _ = window.unminimize();
        let _ = window.set_focus();
    }

    eprintln!("[window] shown: {}", kind.label());
    Ok(())
}

pub fn hide_window(app: &AppHandle, kind: WindowKind) -> tauri::Result<()> {
    if let Some(window) = app.get_webview_window(kind.label()) {
        let _ = window.hide();
    }
    Ok(())
}

/// Position a window using **physical** pixel coordinates.
/// Used by the selection monitor (mouse hook gives physical coords).
pub fn position_window_physical(
    app: &AppHandle,
    kind: WindowKind,
    x: f64,
    y: f64,
) -> tauri::Result<()> {
    let window = ensure_window(app, kind)?;
    window.set_position(Position::Physical(PhysicalPosition::new(
        x.round() as i32,
        y.round() as i32,
    )))?;
    Ok(())
}

/// Position a window using **logical** coordinates.
/// Used by frontend commands (JS gives CSS/logical coords).
pub fn position_window_logical(
    app: &AppHandle,
    kind: WindowKind,
    x: f64,
    y: f64,
) -> tauri::Result<()> {
    let window = ensure_window(app, kind)?;
    window.set_position(Position::Logical(LogicalPosition::new(x, y)))?;
    Ok(())
}

pub fn resize_window(
    app: &AppHandle,
    kind: WindowKind,
    width: f64,
    height: f64,
) -> tauri::Result<()> {
    let window = ensure_window(app, kind)?;
    window.set_size(Size::Logical(LogicalSize::new(
        width.max(1.0),
        height.max(1.0),
    )))?;
    Ok(())
}
