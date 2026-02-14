use crate::ocr::{self, OcrCaptureRegion};

#[tauri::command]
pub fn start_ocr_capture(app: tauri::AppHandle) -> Result<(), String> {
    ocr::open_capture_overlay(&app)
}

#[tauri::command]
pub async fn run_ocr_capture(
    app: tauri::AppHandle,
    region: OcrCaptureRegion,
) -> Result<(), String> {
    ocr::run_ocr_capture(&app, region).await
}

#[tauri::command]
pub fn capture_screenshot_preview(
    app: tauri::AppHandle,
    request: ScreenshotCaptureRequest,
) -> Result<ScreenshotPreviewPayload, String> {
    ocr::capture_screenshot_preview(&app, request)
}

#[tauri::command]
pub fn resolve_window_capture_hint(
    app: tauri::AppHandle,
    point: CapturePoint,
) -> Result<Option<ocr::LogicalRectPayload>, String> {
    ocr::resolve_window_capture_hint(&app, point)
}
