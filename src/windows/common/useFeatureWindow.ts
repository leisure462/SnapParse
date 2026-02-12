import { useEffect, useRef, useState } from "react";
import { getCurrentWindow } from "@tauri-apps/api/window";
import { invoke } from "@tauri-apps/api/core";
import { defaultSettings, type AppSettings } from "../../shared/settings";

export interface FeatureWindowState {
  pinned: boolean;
  fontSize: number;
  onPinToggle: () => void;
  shellStyle: React.CSSProperties;
}

/**
 * Shared hook for feature windows: loads settings, manages pin/always-on-top,
 * opacity (from settings only), and blur-to-close behavior.
 */
export function useFeatureWindow(): FeatureWindowState {
  const [pinned, setPinned] = useState(false);
  const [fontSize, setFontSize] = useState(14);
  const pinnedRef = useRef(false);

  // Load settings on mount
  useEffect(() => {
    invoke<AppSettings>("get_settings")
      .then((s) => {
        const fs = s.window?.fontSize ?? defaultSettings().window.fontSize;
        setFontSize(fs);

        // Apply saved opacity (one-shot, no cycling)
        const savedOpacity = s.window?.opacity ?? 1;
        const win = getCurrentWindow();
        // @ts-ignore - setOpacity exists in Tauri v2 but TypeScript types may be incomplete
        if (typeof win.setOpacity === 'function') {
          // @ts-ignore
          win.setOpacity(savedOpacity).catch(() => {});
        }
      })
      .catch(() => { /* use defaults */ });
  }, []);

  // Blur-to-close: listen for focus changes to hide window when not pinned.
  // Uses a short delay so that dragging the title bar (which briefly blurs the
  // window) does not accidentally close it.
  useEffect(() => {
    let unlisten: (() => void) | undefined;
    let blurTimer: ReturnType<typeof setTimeout> | undefined;

    getCurrentWindow().onFocusChanged(({ payload: focused }) => {
      if (blurTimer !== undefined) {
        clearTimeout(blurTimer);
        blurTimer = undefined;
      }

      if (!focused && !pinnedRef.current) {
        blurTimer = setTimeout(() => {
          getCurrentWindow().hide().catch(() => {});
        }, 180);
      }
    }).then((cleanup) => {
      unlisten = cleanup;
    }).catch(() => {});

    return () => {
      if (blurTimer !== undefined) {
        clearTimeout(blurTimer);
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

  const shellStyle: React.CSSProperties = {
    "--snapparse-font-size": `${fontSize}px`
  } as React.CSSProperties;

  return {
    pinned,
    fontSize,
    onPinToggle,
    shellStyle
  };
}
