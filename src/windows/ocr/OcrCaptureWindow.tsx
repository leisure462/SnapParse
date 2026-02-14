import { invoke } from "@tauri-apps/api/core";
import { getCurrentWindow } from "@tauri-apps/api/window";
import { useEffect, useMemo, useState } from "react";
import "./ocrCapture.css";

interface Point {
  x: number;
  y: number;
}

interface OcrCaptureRegion {
  x: number;
  y: number;
  width: number;
  height: number;
  scaleFactor: number;
}

const MIN_CAPTURE_SIZE = 8;

function toRect(start: Point, end: Point): { x: number; y: number; width: number; height: number } {
  const left = Math.min(start.x, end.x);
  const top = Math.min(start.y, end.y);
  const width = Math.abs(start.x - end.x);
  const height = Math.abs(start.y - end.y);
  return { x: left, y: top, width, height };
}

async function closeCaptureWindow(): Promise<void> {
  try {
    await invoke("close_window", { kind: "ocr-capture" });
  } catch {
    const current = getCurrentWindow() as ReturnType<typeof getCurrentWindow> & {
      hide?: () => Promise<void>;
    };
    if (typeof current.hide === "function") {
      await current.hide();
    }
  }
}

export default function OcrCaptureWindow(): JSX.Element {
  const [startPoint, setStartPoint] = useState<Point | null>(null);
  const [endPoint, setEndPoint] = useState<Point | null>(null);
  const [isDragging, setIsDragging] = useState(false);
  const [processing, setProcessing] = useState(false);

  const rect = useMemo(() => {
    if (!startPoint || !endPoint) {
      return null;
    }
    return toRect(startPoint, endPoint);
  }, [endPoint, startPoint]);

  useEffect(() => {
    const handleKeyDown = (event: KeyboardEvent): void => {
      if (event.key === "Escape" && !processing) {
        void closeCaptureWindow();
      }
    };

    window.addEventListener("keydown", handleKeyDown);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, [processing]);

  const startDrag = (x: number, y: number): void => {
    if (processing) {
      return;
    }
    setStartPoint({ x, y });
    setEndPoint({ x, y });
    setIsDragging(true);
  };

  const updateDrag = (x: number, y: number): void => {
    if (!isDragging || processing) {
      return;
    }
    setEndPoint({ x, y });
  };

  const finishDrag = async (x: number, y: number): Promise<void> => {
    if (!isDragging || !startPoint || processing) {
      return;
    }

    const finalRect = toRect(startPoint, { x, y });
    setIsDragging(false);
    setEndPoint({ x, y });

    if (finalRect.width < MIN_CAPTURE_SIZE || finalRect.height < MIN_CAPTURE_SIZE) {
      setStartPoint(null);
      setEndPoint(null);
      return;
    }

    const payload: OcrCaptureRegion = {
      x: finalRect.x,
      y: finalRect.y,
      width: finalRect.width,
      height: finalRect.height,
      scaleFactor: window.devicePixelRatio || 1
    };

    setProcessing(true);

    try {
      await invoke("run_ocr_capture", { region: payload });
    } catch {
      // Ignore here to avoid leaving overlay in an unusable state.
    } finally {
      await closeCaptureWindow();
    }
  };

  return (
    <main
      className="ocr-capture-shell"
      onMouseDown={(event) => {
        if (event.button !== 0) {
          return;
        }
        startDrag(event.clientX, event.clientY);
      }}
      onMouseMove={(event) => {
        updateDrag(event.clientX, event.clientY);
      }}
      onMouseUp={(event) => {
        if (event.button !== 0) {
          return;
        }
        void finishDrag(event.clientX, event.clientY);
      }}
      onContextMenu={(event) => {
        event.preventDefault();
      }}
      role="application"
      aria-label="OCR截图窗口"
    >
      <div className="ocr-capture-hint">
        {processing
          ? "正在识别文字，请稍候..."
          : "拖拽鼠标框选 OCR 区域，松开后自动识别，Esc 取消"}
      </div>

      {rect ? (
        <div
          className="ocr-capture-rect"
          style={{
            left: `${rect.x}px`,
            top: `${rect.y}px`,
            width: `${rect.width}px`,
            height: `${rect.height}px`
          }}
        />
      ) : null}

    </main>
  );
}
