import { useEffect, useRef, useState } from "react";
import { getCurrentWindow } from "@tauri-apps/api/window";
import { invoke } from "@tauri-apps/api/core";
import { listen, type UnlistenFn } from "@tauri-apps/api/event";
import { defaultSettings, type AppLanguage, type AppSettings } from "../../shared/settings";
import { applyThemeMode, type ThemeMode } from "../theme/themeStore";

export interface FeatureWindowState {
  pinned: boolean;
  fontSize: number;
  language: AppLanguage;
  onPinToggle: () => void;
  shellStyle: React.CSSProperties;
}

/**
 * Shared hook for feature windows: loads settings, manages pin/always-on-top,
 * theme, and blur-to-close behavior.
 *
 * Listens for the `settings-changed` Tauri event so font size and theme
 * update in real-time when the user saves settings – even though feature
 * windows are pre-created at startup and never remount.
 */
export function useFeatureWindow(): FeatureWindowState {
  const [pinned, setPinned] = useState(false);
  const [fontSize, setFontSize] = useState(14);
  const [language, setLanguage] = useState<AppLanguage>(defaultSettings().general.language);
  const pinnedRef = useRef(false);

  /** Apply settings values from an AppSettings object. */
  const applySettings = (s: AppSettings): void => {
    const defaults = defaultSettings();
    const fs = s.window?.fontSize ?? defaults.window.fontSize;
    const tm = (s.toolbar?.themeMode ?? defaults.toolbar.themeMode) as ThemeMode;
    const languageValue = s.general?.language ?? defaults.general.language;
    setFontSize(fs);
    setLanguage(languageValue);
    applyThemeMode(tm);
  };

  // Load settings on mount
  useEffect(() => {
    invoke<AppSettings>("get_settings")
      .then(applySettings)
      .catch(() => {
        // Keep behavior deterministic even if settings load fails.
        applySettings(defaultSettings());
      });
  }, []);

  // Listen for settings-changed events (emitted when the user saves settings)
  useEffect(() => {
    let unlisten: UnlistenFn | undefined;

    listen<AppSettings>("settings-changed", (event) => {
      applySettings(event.payload);
    }).then((cleanup) => {
      unlisten = cleanup;
    });

    return () => {
      unlisten?.();
    };
  }, []);

  // Blur-to-close: listen for focus changes to hide window when not pinned.
  // Uses a short delay so that dragging the title bar (which briefly blurs the
  // window) does not accidentally close it.
  useEffect(() => {
    let unlisten: (() => void) | undefined;
    let blurTimer: ReturnType<typeof setTimeout> | undefined;
    let keepOnTopInterval: ReturnType<typeof setInterval> | undefined;

    getCurrentWindow().onFocusChanged(({ payload: focused }) => {
      if (blurTimer !== undefined) {
        clearTimeout(blurTimer);
        blurTimer = undefined;
      }

      if (!focused && !pinnedRef.current) {
        blurTimer = setTimeout(() => {
          getCurrentWindow().hide().catch(() => {});
        }, 180);
      } else if (!focused && pinnedRef.current) {
        getCurrentWindow().setAlwaysOnTop(true).catch(() => {});
        if (keepOnTopInterval === undefined) {
          keepOnTopInterval = setInterval(() => {
            if (pinnedRef.current) {
              getCurrentWindow().setAlwaysOnTop(true).catch(() => {});
            }
          }, 1000);
        }
      } else if (focused) {
        if (keepOnTopInterval !== undefined) {
          clearInterval(keepOnTopInterval);
          keepOnTopInterval = undefined;
        }
      }
    }).then((cleanup) => {
      unlisten = cleanup;
    }).catch(() => {});

    return () => {
      if (blurTimer !== undefined) {
        clearTimeout(blurTimer);
      }
      if (keepOnTopInterval !== undefined) {
        clearInterval(keepOnTopInterval);
      }
      unlisten?.();
    };
  }, []);

  const onPinToggle = (): void => {
    const next = !pinned;
    setPinned(next);
    pinnedRef.current = next;
    getCurrentWindow().setAlwaysOnTop(next).catch(() => {});
  };

  useEffect(() => {
    getCurrentWindow().setAlwaysOnTop(pinned).catch(() => {});
  }, [pinned]);

  const shellStyle: React.CSSProperties = {
    "--snapparse-font-size": `${fontSize}px`,
  } as React.CSSProperties;

  return {
    pinned,
    fontSize,
    language,
    onPinToggle,
    shellStyle
  };
}
