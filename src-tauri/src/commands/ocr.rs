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
