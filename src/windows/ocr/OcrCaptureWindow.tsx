import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import { getCurrentWindow } from "@tauri-apps/api/window";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import type { AppSettings, CaptureMode } from "../../shared/settings";
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

interface CapturePoint {
  x: number;
  y: number;
  scaleFactor: number;
}

interface LogicalRect {
  x: number;
  y: number;
  width: number;
  height: number;
}

interface ScreenshotPreviewPayload {
  dataUrl: string;
  logicalRect: LogicalRect;
}

interface CaptureOpenedPayload {
  entryKind?: "screenshot" | "ocr";
}

interface CaptureSettings {
  defaultMode: CaptureMode;
  showShortcutHints: boolean;
  modeHotkeys: {
    region: string;
    fullscreen: string;
    window: string;
  };
}

const MIN_CAPTURE_SIZE = 8;
const DEFAULT_CAPTURE_SETTINGS: CaptureSettings = {
  defaultMode: "region",
  showShortcutHints: true,
  modeHotkeys: {
    region: "Ctrl+R",
    fullscreen: "Ctrl+A",
    window: "Ctrl+M"
  }
};

function toRect(start: Point, end: Point): { x: number; y: number; width: number; height: number } {
  const left = Math.min(start.x, end.x);
  const top = Math.min(start.y, end.y);
  const width = Math.abs(start.x - end.x);
  const height = Math.abs(start.y - end.y);
  return { x: left, y: top, width, height };
}

function normalizeKey(key: string): string | null {
  if (key === " ") {
    return "Space";
  }

  if (key.length === 1) {
    return key.toUpperCase();
  }

  const keyMap: Record<string, string> = {
    Escape: "Esc",
    Enter: "Enter",
    Backspace: "Backspace",
    Delete: "Delete",
    ArrowUp: "Up",
    ArrowDown: "Down",
    ArrowLeft: "Left",
    ArrowRight: "Right"
  };

  if (keyMap[key]) {
    return keyMap[key];
  }

  if (/^F\d{1,2}$/.test(key)) {
    return key;
  }

  return null;
}

function matchesHotkey(event: KeyboardEvent, hotkey: string): boolean {
  const raw = hotkey.trim();
  if (!raw) {
    return false;
  }

  const segments = raw.split("+").map((item) => item.trim()).filter(Boolean);
  if (segments.length === 0) {
    return false;
  }

  const key = segments[segments.length - 1];
  const modifiers = new Set(segments.slice(0, -1));
  const normalized = normalizeKey(event.key);
  if (!normalized || normalized !== key) {
    return false;
  }

  const expectCtrl = modifiers.has("Ctrl");
  const expectShift = modifiers.has("Shift");
  const expectAlt = modifiers.has("Alt");
  const expectMeta = modifiers.has("Meta");

  return (
    event.ctrlKey === expectCtrl &&
    event.shiftKey === expectShift &&
    event.altKey === expectAlt &&
    event.metaKey === expectMeta
  );
}

function toOcrRegion(rect: LogicalRect): OcrCaptureRegion {
  return {
    x: rect.x,
    y: rect.y,
    width: rect.width,
    height: rect.height,
    scaleFactor: window.devicePixelRatio || 1
  };
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
  const [mode, setMode] = useState<CaptureMode>(DEFAULT_CAPTURE_SETTINGS.defaultMode);
  const [captureSettings, setCaptureSettings] = useState<CaptureSettings>(DEFAULT_CAPTURE_SETTINGS);
  const [startPoint, setStartPoint] = useState<Point | null>(null);
  const [endPoint, setEndPoint] = useState<Point | null>(null);
  const [isDragging, setIsDragging] = useState(false);
  const [processing, setProcessing] = useState(false);
  const [windowHintRect, setWindowHintRect] = useState<LogicalRect | null>(null);
  const [captured, setCaptured] = useState<{ dataUrl: string; logicalRect: LogicalRect } | null>(null);
  const [actionBusy, setActionBusy] = useState<"ocr" | "copy" | "save" | null>(null);
  const [entryKind, setEntryKind] = useState<"screenshot" | "ocr">("screenshot");
  const windowHintTimerRef = useRef<number | null>(null);
  const latestPointerRef = useRef<Point | null>(null);
  const settingsRef = useRef<CaptureSettings>(DEFAULT_CAPTURE_SETTINGS);

  const resetCaptureState = useCallback((): void => {
    setProcessing(false);
    setIsDragging(false);
    setStartPoint(null);
    setEndPoint(null);
    setWindowHintRect(null);
    setCaptured(null);
    setActionBusy(null);

    if (windowHintTimerRef.current !== null) {
      window.clearTimeout(windowHintTimerRef.current);
      windowHintTimerRef.current = null;
    }
  }, []);

  const loadSettings = useCallback((settings: AppSettings): void => {
    const next: CaptureSettings = {
      defaultMode: settings.ocr.captureDefaultMode,
      showShortcutHints: settings.ocr.showShortcutHints,
      modeHotkeys: {
        region: settings.ocr.modeHotkeys.region,
        fullscreen: settings.ocr.modeHotkeys.fullscreen,
        window: settings.ocr.modeHotkeys.window
      }
    };
    settingsRef.current = next;
    setCaptureSettings(next);
  }, []);

  const rect = useMemo(() => {
    if (!startPoint || !endPoint) {
      return null;
    }
    return toRect(startPoint, endPoint);
  }, [endPoint, startPoint]);

  const activeRect = captured?.logicalRect ?? (mode === "window" ? windowHintRect : rect);

  const actionsStyle = useMemo(() => {
    if (!activeRect) {
      return undefined;
    }

    const panelWidth = 172;
    const panelHeight = 36;
    const gap = 8;

    let left = activeRect.x;
    let top = activeRect.y + activeRect.height + gap;

    if (left + panelWidth > window.innerWidth - gap) {
      left = Math.max(gap, window.innerWidth - panelWidth - gap);
    }

    if (top + panelHeight > window.innerHeight - gap) {
      top = Math.max(gap, activeRect.y - panelHeight - gap);
    }

    return {
      left: `${Math.round(left)}px`,
      top: `${Math.round(top)}px`
    };
  }, [activeRect]);

  const capturePreview = useCallback(async (request: {
    mode: CaptureMode;
    region?: OcrCaptureRegion;
    point?: CapturePoint;
  }): Promise<void> => {
    setProcessing(true);

    try {
      const result = await invoke<ScreenshotPreviewPayload>("capture_screenshot_preview", { request });
      setCaptured({
        dataUrl: result.dataUrl,
        logicalRect: result.logicalRect
      });
      setWindowHintRect(result.logicalRect);
    } finally {
      setProcessing(false);
      setIsDragging(false);
      setStartPoint(null);
      setEndPoint(null);
    }
  }, []);

  const updateWindowHint = useCallback((point: Point): void => {
    latestPointerRef.current = point;

    if (windowHintTimerRef.current !== null || captured) {
      return;
    }

    windowHintTimerRef.current = window.setTimeout(() => {
      windowHintTimerRef.current = null;
      const currentPoint = latestPointerRef.current;
      if (!currentPoint || mode !== "window" || captured) {
        return;
      }

      void invoke<LogicalRect | null>("resolve_window_capture_hint", {
        point: {
          x: currentPoint.x,
          y: currentPoint.y,
          scaleFactor: window.devicePixelRatio || 1
        }
      })
        .then((value) => {
          setWindowHintRect(value);
        })
        .catch(() => {
          setWindowHintRect(null);
        });
    }, 45);
  }, [captured, mode]);

  const runFullscreenCapture = useCallback((): void => {
    if (processing || captured) {
      return;
    }

    void capturePreview({ mode: "fullscreen" });
  }, [capturePreview, captured, processing]);

  const runWindowCapture = useCallback((point: Point): void => {
    if (processing || captured) {
      return;
    }

    void capturePreview({
      mode: "window",
      point: {
        x: point.x,
        y: point.y,
        scaleFactor: window.devicePixelRatio || 1
      }
    });
  }, [capturePreview, captured, processing]);

  const runRegionCapture = useCallback((regionRect: LogicalRect): void => {
    if (processing || captured) {
      return;
    }

    void capturePreview({
      mode: "region",
      region: toOcrRegion(regionRect)
    });
  }, [capturePreview, captured, processing]);

  useEffect(() => {
    let unlisten: (() => void) | undefined;

    listen<CaptureOpenedPayload>("ocr-capture-opened", (event) => {
      resetCaptureState();
      const incoming = event.payload?.entryKind === "ocr" ? "ocr" : "screenshot";
      setEntryKind(incoming);
      setMode(incoming === "ocr" ? "region" : settingsRef.current.defaultMode);
    }).then((cleanup) => {
      unlisten = cleanup;
    });

    return () => {
      unlisten?.();
    };
  }, [resetCaptureState]);

  useEffect(() => {
    let cancelled = false;

    void invoke<AppSettings>("get_settings")
      .then((value) => {
        if (cancelled) {
          return;
        }
        loadSettings(value);
        setMode(value.ocr.captureDefaultMode);
      })
      .catch(() => {});

    let unlisten: (() => void) | undefined;
    listen<AppSettings>("settings-changed", (event) => {
      loadSettings(event.payload);
    }).then((cleanup) => {
      unlisten = cleanup;
    });

    return () => {
      cancelled = true;
      unlisten?.();
    };
  }, [loadSettings]);

  useEffect(() => {
    const handleKeyDown = (event: KeyboardEvent): void => {
      if (event.key === "Escape") {
        event.preventDefault();
        resetCaptureState();
        void closeCaptureWindow();
        return;
      }

      if (matchesHotkey(event, captureSettings.modeHotkeys.fullscreen)) {
        event.preventDefault();
        setMode("fullscreen");
        runFullscreenCapture();
        return;
      }

      if (matchesHotkey(event, captureSettings.modeHotkeys.window)) {
        event.preventDefault();
        setMode("window");
        setCaptured(null);
        return;
      }

      if (matchesHotkey(event, captureSettings.modeHotkeys.region)) {
        event.preventDefault();
        setMode("region");
        setCaptured(null);
      }
    };

    window.addEventListener("keydown", handleKeyDown);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, [captureSettings.modeHotkeys.fullscreen, captureSettings.modeHotkeys.region, captureSettings.modeHotkeys.window, resetCaptureState, runFullscreenCapture]);

  const startDrag = (x: number, y: number): void => {
    if (processing || captured || mode !== "region") {
      return;
    }
    setStartPoint({ x, y });
    setEndPoint({ x, y });
    setIsDragging(true);
  };

  const updateDrag = (x: number, y: number): void => {
    if (mode === "window") {
      updateWindowHint({ x, y });
      return;
    }

    if (!isDragging || processing || captured) {
      return;
    }
    setEndPoint({ x, y });
  };

  const finishDrag = async (x: number, y: number): Promise<void> => {
    if (captured || processing) {
      return;
    }

    if (mode === "window") {
      runWindowCapture({ x, y });
      return;
    }

    if (mode === "fullscreen") {
      runFullscreenCapture();
      return;
    }

    if (!isDragging || !startPoint) {
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

    const payload: LogicalRect = {
      x: finalRect.x,
      y: finalRect.y,
      width: finalRect.width,
      height: finalRect.height
    };

    if (entryKind === "ocr") {
      setProcessing(true);
      try {
        await invoke("run_ocr_capture", { region: toOcrRegion(payload) });
      } catch {
        // noop
      } finally {
        resetCaptureState();
        await closeCaptureWindow();
      }
      return;
    }

    runRegionCapture(payload);
  };

  const runOcrAction = useCallback(async (): Promise<void> => {
    if (!captured || actionBusy || processing) {
      return;
    }

    setActionBusy("ocr");
    try {
      await invoke("run_ocr_capture", { region: toOcrRegion(captured.logicalRect) });
      resetCaptureState();
      await closeCaptureWindow();
    } finally {
      setActionBusy(null);
    }
  }, [actionBusy, captured, processing, resetCaptureState]);

  const copyAction = useCallback(async (): Promise<void> => {
    if (!captured || actionBusy) {
      return;
    }

    setActionBusy("copy");
    try {
      const response = await fetch(captured.dataUrl);
      const blob = await response.blob();
      if (typeof ClipboardItem !== "undefined") {
        await navigator.clipboard.write([new ClipboardItem({ [blob.type]: blob })]);
      } else {
        await navigator.clipboard.writeText(captured.dataUrl);
      }
    } catch {
      await navigator.clipboard.writeText(captured.dataUrl);
    } finally {
      setActionBusy(null);
    }
  }, [actionBusy, captured]);

  const saveAction = useCallback(async (): Promise<void> => {
    if (!captured || actionBusy) {
      return;
    }

    setActionBusy("save");
    try {
      const stamp = new Date()
        .toISOString()
        .replace(/[-:]/g, "")
        .replace(/\.\d+Z$/, "")
        .replace("T", "_");
      const anchor = document.createElement("a");
      anchor.href = captured.dataUrl;
      anchor.download = `SnapParse_${stamp}.png`;
      document.body.append(anchor);
      anchor.click();
      anchor.remove();
    } finally {
      setActionBusy(null);
    }
  }, [actionBusy, captured]);

  return (
    <main
      className="ocr-capture-shell"
      onMouseDown={(event) => {
        if (event.button !== 0) {
          return;
        }
        if (captured) {
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
      aria-label="截屏窗口"
    >
      <div className="ocr-capture-hint">
        {processing
          ? "正在截取图像..."
          : mode === "region"
            ? "区域模式：拖拽选择截图范围"
            : mode === "fullscreen"
              ? "全屏模式：点击任意位置或按对应快捷键立即截取"
              : "窗口模式：悬停高亮窗口，点击完成截取"}
      </div>

      <div className={`ocr-capture-mode-pill mode-${mode}`}>
        {mode === "region" ? "滑动截屏" : mode === "fullscreen" ? "全屏截屏" : "窗口截屏"}
      </div>

      {captured ? (
        <img
          className="ocr-capture-preview-image"
          src={captured.dataUrl}
          alt="截图预览"
          style={{
            left: `${captured.logicalRect.x}px`,
            top: `${captured.logicalRect.y}px`,
            width: `${captured.logicalRect.width}px`,
            height: `${captured.logicalRect.height}px`
          }}
        />
      ) : null}

      {activeRect ? (
        <div
          className={`ocr-capture-rect ${mode === "window" ? "window-hint" : ""}`}
          style={{
            left: `${activeRect.x}px`,
            top: `${activeRect.y}px`,
            width: `${activeRect.width}px`,
            height: `${activeRect.height}px`
          }}
        />
      ) : null}

      {captureSettings.showShortcutHints ? (
        <div className="ocr-capture-shortcuts">
          <div>{captureSettings.modeHotkeys.fullscreen} 全屏</div>
          <div>{captureSettings.modeHotkeys.window} 窗口</div>
          <div>{captureSettings.modeHotkeys.region} 区域</div>
          <div>Esc 退出截屏</div>
        </div>
      ) : null}

      {captured ? (
        <div className="ocr-capture-actions" style={actionsStyle}>
          <button
            type="button"
            className="ocr-capture-action-btn icon-btn"
            onClick={() => {
              void runOcrAction();
            }}
            disabled={actionBusy !== null || processing}
            aria-label="OCR识别"
            title="OCR识别"
          >
            <svg viewBox="0 0 24 24" width="15" height="15" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
              <rect x="4" y="5" width="16" height="14" rx="2" />
              <path d="M8 9h3" />
              <path d="M8 13h3" />
              <path d="M14 13c0-1.7 1.3-3 3-3" />
              <path d="M17 10v6" />
            </svg>
          </button>
          <button
            type="button"
            className="ocr-capture-action-btn"
            onClick={() => {
              void copyAction();
            }}
            disabled={actionBusy !== null || processing}
            aria-label="复制截图"
            title="复制截图"
          >
            <svg viewBox="0 0 24 24" width="15" height="15" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
              <rect x="8" y="8" width="11" height="12" rx="2" />
              <path d="M5 15V6a2 2 0 0 1 2-2h8" />
            </svg>
          </button>
          <button
            type="button"
            className="ocr-capture-action-btn"
            onClick={() => {
              void saveAction();
            }}
            disabled={actionBusy !== null || processing}
            aria-label="另存为"
            title="另存为"
          >
            <svg viewBox="0 0 24 24" width="15" height="15" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
              <path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z" />
              <path d="M17 21v-8H7v8" />
              <path d="M7 3v5h8" />
            </svg>
          </button>
          <button
            type="button"
            className="ocr-capture-action-btn ghost"
            onClick={() => {
              resetCaptureState();
              void closeCaptureWindow();
            }}
            disabled={actionBusy !== null || processing}
            aria-label="取消"
            title="取消"
          >
            <svg viewBox="0 0 24 24" width="15" height="15" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
              <path d="M18 6 6 18" />
              <path d="m6 6 12 12" />
            </svg>
          </button>
        </div>
      ) : null}

    </main>
  );
}
