import { invoke } from "@tauri-apps/api/core";
import { emit, listen } from "@tauri-apps/api/event";
import { useEffect, useRef, useState } from "react";
import { resolveActionBarActions, type ActionBarAction } from "./actions";
import "./actionBar.css";
import { useThemeMode, type ThemeMode } from "../theme/themeStore";
import { defaultSettings, resolveWindowSize, validateSettings, type AppSettings } from "../../shared/settings";
import { renderActionIcon } from "../common/actionIcon";
// The icon_transparent.png is copied to public/ for Vite to serve at runtime.
const APP_ICON_URL = "/icon_transparent.png";
const LAST_SELECTED_TEXT_KEY = "snapparse:selected-text";

const FEATURE_WINDOW_GAP = 12;
const FEATURE_WINDOW_PADDING = 8;
const ACTION_BAR_ICON_ANIMATION_DELAY_MS = 50;

interface SelectionTextPayload {
  text: string;
  source?: string;
  autoActionId?: string;
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
  const [actions, setActions] = useState<ActionBarAction[]>(() => resolveActionBarActions(defaultSettings()));
  const [autoActionRequest, setAutoActionRequest] = useState<{ text: string; actionId: string } | null>(null);
  const actionBarRef = useRef<HTMLDivElement | null>(null);
  const selectedTextRef = useRef("");
  const iconRef = useRef<HTMLImageElement | null>(null);
  const iconAnimationTimerRef = useRef<number | null>(null);
  const featureWindowSize = useRef({ width: 680, height: 520 });
  const theme = useThemeMode();

  const replayIconAnimation = (): void => {
    const icon = iconRef.current;
    if (!icon) {
      return;
    }

    icon.classList.remove("md2-action-bar-icon--animated");
    void icon.offsetWidth;
    icon.classList.add("md2-action-bar-icon--animated");
  };

  const queueIconAnimation = (): void => {
    if (iconAnimationTimerRef.current !== null) {
      window.clearTimeout(iconAnimationTimerRef.current);
    }

    iconAnimationTimerRef.current = window.setTimeout(() => {
      replayIconAnimation();
      iconAnimationTimerRef.current = null;
    }, ACTION_BAR_ICON_ANIMATION_DELAY_MS);
  };

  useEffect(() => {
    const handleVisibilityChange = (): void => {
      if (document.visibilityState === "visible") {
        queueIconAnimation();
      }
    };

    window.addEventListener("focus", queueIconAnimation);
    document.addEventListener("visibilitychange", handleVisibilityChange);

    return () => {
      window.removeEventListener("focus", queueIconAnimation);
      document.removeEventListener("visibilitychange", handleVisibilityChange);

      if (iconAnimationTimerRef.current !== null) {
        window.clearTimeout(iconAnimationTimerRef.current);
        iconAnimationTimerRef.current = null;
      }
    };
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
  }, [actions, isBusy, selectedText]);

  useEffect(() => {
    let unlisten: (() => void) | undefined;

    listen<SelectionTextPayload>("selection-text-changed", (event) => {
      if (typeof event.payload.text === "string") {
        setSelectedText(event.payload.text);
        selectedTextRef.current = event.payload.text;
        if (event.payload.text.trim()) {
          window.localStorage.setItem(LAST_SELECTED_TEXT_KEY, event.payload.text);
        }
        queueIconAnimation();

        const autoActionId = event.payload.autoActionId?.trim();
        if (event.payload.source === "ocr" && autoActionId) {
          setAutoActionRequest({
            text: event.payload.text,
            actionId: autoActionId
          });
        }
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
        setActions(resolveActionBarActions(normalized));
      } catch {
        if (cancelled) {
          return;
        }

        const defaults = defaultSettings();
        theme.setMode(defaults.toolbar.themeMode as ThemeMode);
        featureWindowSize.current = resolveWindowSize(defaults.window.windowSize);
        setActions(resolveActionBarActions(defaults));
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
      setActions(resolveActionBarActions(normalized));
    }).then((cleanup) => {
      unlisten = cleanup;
    });

    return () => {
      unlisten?.();
    };
  }, []);

  const runAction = async (action: ActionBarAction, directText?: string): Promise<void> => {
    if (isBusy) {
      return;
    }

    setBusy(true);

    try {
      const direct = directText?.trim() ?? "";
      const textForAction =
        direct ||
        selectedTextRef.current.trim() ||
        selectedText.trim() ||
        window.localStorage.getItem(LAST_SELECTED_TEXT_KEY)?.trim() ||
        "";

      if (action.commandWindow) {
        if (!textForAction) {
          await closeActionBarWindow();
          return;
        }

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

        const requestId = Date.now();

        const payload = {
          text: textForAction,
          source: "action-bar",
          target: action.commandWindow,
          title: action.label,
          customPrompt: action.prompt,
          customModel: action.model,
          requestId
        };

        const setPendingOptimizePromise =
          action.commandWindow === "optimize"
            ? invoke("set_pending_optimize_request", { payload }).catch((error) => {
                console.error("[ActionBar] failed to set pending optimize request:", error);
              })
            : Promise.resolve();

        await setPendingOptimizePromise;

        // Emit multiple times with same requestId to reduce first-open race conditions.
        const emitDelays = [0, 120, 300];
        for (const delay of emitDelays) {
          window.setTimeout(() => {
            void emit("change-text", payload);
            if (action.commandWindow === "optimize") {
              void emit("optimize-pending-updated", { requestId });
            }
          }, delay);
        }

        try {
          await invoke("open_window", { kind: action.commandWindow });

          void invoke("resize_window", {
            kind: action.commandWindow,
            width: fwSize.width,
            height: fwSize.height
          }).catch(() => {});

          void invoke("move_window", {
            kind: action.commandWindow,
            x: anchor.x,
            y: anchor.y
          }).catch(() => {});
        } catch (err) {
          console.error("[ActionBar] failed to open/move feature window:", err);
        }

        await closeActionBarWindow();
        return;
      }

      if (action.id === "search") {
        if (textForAction) {
          const query = encodeURIComponent(textForAction);
          const searchUrl = `https://www.google.com/search?q=${query}`;
          try {
            await invoke("open_external_url", { url: searchUrl });
          } catch {
            window.open(searchUrl, "_blank");
          }
        }
        await closeActionBarWindow();
        return;
      }

      if (action.id === "copy") {
        await copyText(textForAction);
        await closeActionBarWindow();
      }
    } finally {
      setBusy(false);
    }
  };

  useEffect(() => {
    if (!autoActionRequest || isBusy) {
      return;
    }

    const targetAction = actions.find((item) => item.id === autoActionRequest.actionId);
    setAutoActionRequest(null);

    if (!targetAction) {
      return;
    }

    void runAction(targetAction, autoActionRequest.text);
  }, [actions, autoActionRequest, isBusy]);

  return (
    <div ref={actionBarRef} className="md2-action-bar" role="toolbar" aria-label="划词工具栏">
      <img ref={iconRef} src={APP_ICON_URL} alt="" className="md2-action-bar-icon" draggable={false} />
      <div className="md2-action-list">
        {actions.map((action) => (
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
              {renderActionIcon(action.icon, 16)}
            </span>
            <span className="md2-action-label">{action.label}</span>
          </button>
        ))}
      </div>
    </div>
  );
}
