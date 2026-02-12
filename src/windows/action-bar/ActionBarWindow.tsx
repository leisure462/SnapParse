import { invoke } from "@tauri-apps/api/core";
import { emit, listen } from "@tauri-apps/api/event";
import { useEffect, useRef, useState } from "react";
import { DEFAULT_ACTIONS, type ActionBarAction, type ActionBarActionId } from "./actions";
import "./actionBar.css";
import { useThemeMode, type ThemeMode } from "../theme/themeStore";
import { defaultSettings, resolveWindowSize, validateSettings, type AppSettings } from "../../shared/settings";
// The icon_transparent.png is copied to public/ for Vite to serve at runtime.
const APP_ICON_URL = "/icon_transparent.png";

const LAST_SELECTED_TEXT_KEY = "snapparse:selected-text";
const FEATURE_WINDOW_GAP = 12;
const FEATURE_WINDOW_PADDING = 8;

interface SelectionTextPayload {
  text: string;
  source?: string;
}

function iconForAction(id: ActionBarActionId): JSX.Element {
  const common = { viewBox: "0 0 24 24", width: 16, height: 16, fill: "none", stroke: "currentColor" };

  if (id === "translate") {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <path d="M4 5h10" />
        <path d="M9 5c0 6-3 9-6 11" />
        <path d="M9 11c1.5 2.2 3.3 4 5.2 5.4" />
        <path d="M14 5h6" />
        <path d="M18 5v10" />
      </svg>
    );
  }

  if (id === "explain") {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <path d="M12 18h.01" />
        <path d="M9 9a3 3 0 1 1 6 0c0 2-3 2-3 5" />
        <rect x="3" y="3" width="18" height="18" rx="3" />
      </svg>
    );
  }

  if (id === "summarize") {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <path d="M5 6h14" />
        <path d="M5 12h10" />
        <path d="M5 18h6" />
      </svg>
    );
  }

  if (id === "search") {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <circle cx="11" cy="11" r="7" />
        <path d="m20 20-3.8-3.8" />
      </svg>
    );
  }

  return (
    <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <rect x="8" y="8" width="11" height="12" rx="2" />
      <path d="M5 15V6a2 2 0 0 1 2-2h8" />
    </svg>
  );
}

async function copyText(value: string): Promise<void> {
  if (!value.trim()) {
    return;
  }

  try {
    await navigator.clipboard.writeText(value);
  } catch {
    const fallback = document.createElement("textarea");
    fallback.value = value;
    fallback.style.position = "fixed";
    fallback.style.opacity = "0";
    document.body.append(fallback);
    fallback.select();
    document.execCommand("copy");
    fallback.remove();
  }
}

async function closeActionBarWindow(): Promise<void> {
  await invoke("close_window", { kind: "action-bar" });
}

function computeFeatureWindowAnchor(
  actionBarElement: HTMLElement | null | undefined,
  featureWidth: number,
  featureHeight: number
): { x: number; y: number } {
  const screenLike = window.screen as Screen & {
    availLeft?: number;
    availTop?: number;
  };

  const availLeft = Number.isFinite(screenLike.availLeft) ? Number(screenLike.availLeft) : 0;
  const availTop = Number.isFinite(screenLike.availTop) ? Number(screenLike.availTop) : 0;

  const actionBarRect = actionBarElement?.getBoundingClientRect();
  const actionBarWidth = actionBarRect?.width ?? 402;
  const actionBarHeight = actionBarRect?.height ?? 48;

  const rawX = Math.round(window.screenX + actionBarWidth / 2 - featureWidth / 2);
  const rawY = Math.round(window.screenY + actionBarHeight + FEATURE_WINDOW_GAP);

  const minX = availLeft + FEATURE_WINDOW_PADDING;
  const maxX =
    availLeft + window.screen.availWidth - featureWidth - FEATURE_WINDOW_PADDING;

  const minY = availTop + FEATURE_WINDOW_PADDING;
  const maxY =
    availTop + window.screen.availHeight - featureHeight - FEATURE_WINDOW_PADDING;

  const x = Math.min(Math.max(rawX, minX), Math.max(minX, maxX));
  const y = Math.min(Math.max(rawY, minY), Math.max(minY, maxY));

  return { x, y };
}

async function resizeActionBarWindow(element: HTMLElement): Promise<void> {
  const rect = element.getBoundingClientRect();
  const width = Math.ceil(rect.width);
  const height = Math.ceil(rect.height);

  await invoke("resize_window", {
    kind: "action-bar",
    width,
    height
  });
}

export default function ActionBarWindow(): JSX.Element {
  const [selectedText, setSelectedText] = useState("");
  const [isBusy, setBusy] = useState(false);
  const actionBarRef = useRef<HTMLElement | null>(null);
  const featureWindowSize = useRef({ width: 680, height: 520 });
  const theme = useThemeMode();

  useEffect(() => {
    document.documentElement.style.background = "transparent";
    document.body.style.background = "transparent";
  }, []);

  useEffect(() => {
    const syncWindowSize = (): void => {
      const element = actionBarRef.current;
      if (!element) {
        return;
      }

      void resizeActionBarWindow(element).catch(() => {
        // noop in browser test runtime
      });
    };

    const raf = window.requestAnimationFrame(syncWindowSize);
    const timerA = window.setTimeout(syncWindowSize, 120);
    const timerB = window.setTimeout(syncWindowSize, 360);

    return () => {
      window.cancelAnimationFrame(raf);
      window.clearTimeout(timerA);
      window.clearTimeout(timerB);
    };
  }, []);

  useEffect(() => {
    const timer = window.setTimeout(() => {
      const element = actionBarRef.current;
      if (!element) {
        return;
      }

      void resizeActionBarWindow(element).catch(() => {
        // noop in browser test runtime
      });
    }, 24);

    return () => {
      window.clearTimeout(timer);
    };
  }, [selectedText]);

  useEffect(() => {
    let unlisten: (() => void) | undefined;

    listen<SelectionTextPayload>("selection-text-changed", (event) => {
      if (typeof event.payload.text === "string") {
        setSelectedText(event.payload.text);
      }
    }).then((cleanup) => {
      unlisten = cleanup;
    });

    return () => {
      unlisten?.();
    };
  }, []);

  useEffect(() => {
    let cancelled = false;

    const loadSettings = async (): Promise<void> => {
      try {
        const loaded = await invoke<AppSettings>("get_settings");
        if (cancelled) {
          return;
        }

        const normalized = validateSettings(loaded as Partial<AppSettings>);
        theme.setMode(normalized.toolbar.themeMode as ThemeMode);
        const size = resolveWindowSize(normalized.window.windowSize);
        featureWindowSize.current = size;
      } catch {
        if (cancelled) {
          return;
        }

        const defaults = defaultSettings();
        theme.setMode(defaults.toolbar.themeMode as ThemeMode);
        featureWindowSize.current = resolveWindowSize(defaults.window.windowSize);
      }
    };

    void loadSettings();

    return () => {
      cancelled = true;
    };
  }, []);

  // Re-sync theme (and window-size preset) whenever the user saves settings.
  useEffect(() => {
    let unlisten: (() => void) | undefined;

    listen<AppSettings>("settings-changed", (event) => {
      const normalized = validateSettings(event.payload as Partial<AppSettings>);
      theme.setMode(normalized.toolbar.themeMode as ThemeMode);
      featureWindowSize.current = resolveWindowSize(normalized.window.windowSize);
    }).then((cleanup) => {
      unlisten = cleanup;
    });

    return () => {
      unlisten?.();
    };
  }, []);

  const runAction = async (action: ActionBarAction): Promise<void> => {
    if (isBusy) {
      return;
    }

    setBusy(true);

    try {
      if (action.commandWindow) {
        // Re-read settings every time so the latest window-size preset is used
        try {
          const freshSettings = await invoke<AppSettings>("get_settings");
          const normalized = validateSettings(freshSettings as Partial<AppSettings>);
          featureWindowSize.current = resolveWindowSize(normalized.window.windowSize);
        } catch {
          // keep whatever is cached
        }

        const fwSize = featureWindowSize.current;
        const anchor = computeFeatureWindowAnchor(actionBarRef.current, fwSize.width, fwSize.height);

        if (selectedText.trim()) {
          window.localStorage.setItem(LAST_SELECTED_TEXT_KEY, selectedText);
        }

        try {
          await invoke("open_window", { kind: action.commandWindow });
          await invoke("resize_window", {
            kind: action.commandWindow,
            width: fwSize.width,
            height: fwSize.height
          });
          await invoke("move_window", {
            kind: action.commandWindow,
            x: anchor.x,
            y: anchor.y
          });
        } catch (err) {
          console.error("[ActionBar] failed to open/move feature window:", err);
        }

        // Delay event emission to give the target window time to mount and register listeners.
        // The target window also reads from localStorage as a fallback.
        setTimeout(() => {
          void emit("change-text", { text: selectedText, source: "action-bar" });
        }, 300);

        await closeActionBarWindow();
        return;
      }

      if (action.id === "search") {
        if (selectedText.trim()) {
          const query = encodeURIComponent(selectedText);
          window.open(`https://www.google.com/search?q=${query}`, "_blank");
        }
        await closeActionBarWindow();
        return;
      }

      if (action.id === "copy") {
        await copyText(selectedText);
        await closeActionBarWindow();
      }
    } finally {
      setBusy(false);
    }
  };

  return (
    <section ref={actionBarRef} className="md2-action-shell">
      <div className="md2-action-bar" role="toolbar" aria-label="划词工具栏">
        <img src={APP_ICON_URL} alt="" className="md2-action-bar-icon" draggable={false} />
        <div className="md2-action-list">
          {DEFAULT_ACTIONS.map((action) => (
            <button
              key={action.id}
              type="button"
              className="md2-action-btn"
              onClick={() => {
                void runAction(action);
              }}
              disabled={isBusy}
            >
              <span className="md2-action-icon" aria-hidden="true">
                {iconForAction(action.id)}
              </span>
              <span className="md2-action-label">{action.label}</span>
            </button>
          ))}
        </div>
      </div>
    </section>
  );
}
