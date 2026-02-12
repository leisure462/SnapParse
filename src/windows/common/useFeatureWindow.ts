import { useEffect, useRef, useState } from "react";
import { getCurrentWindow } from "@tauri-apps/api/window";
import { invoke } from "@tauri-apps/api/core";
import { defaultSettings, resolveWindowSize, type AppSettings, type WindowSizePreset } from "../../shared/settings";

const OPACITY_LEVELS = [1, 0.85, 0.7, 0.5];

export interface FeatureWindowState {
  pinned: boolean;
  fontSize: number;
  onPinToggle: () => void;
  onOpacityCycle: () => void;
  shellStyle: React.CSSProperties;
}

/**
 * Shared hook for feature windows: loads settings, manages pin/always-on-top,
 * opacity cycling, and blur-to-close behavior.
 */
export function useFeatureWindow(): FeatureWindowState {
  const [pinned, setPinned] = useState(false);
  const [fontSize, setFontSize] = useState(14);
  const [opacityIndex, setOpacityIndex] = useState(0);
  const pinnedRef = useRef(false);

  // Load settings on mount
  useEffect(() => {
    invoke<AppSettings>("get_settings")
      .then((s) => {
        const fs = s.window?.fontSize ?? defaultSettings().window.fontSize;
        setFontSize(fs);

        // Apply saved opacity
        const savedOpacity = s.window?.opacity ?? 1;
        const closestIdx = OPACITY_LEVELS.reduce((best, val, idx) =>
          Math.abs(val - savedOpacity) < Math.abs(OPACITY_LEVELS[best] - savedOpacity) ? idx : best, 0);
        setOpacityIndex(closestIdx);
      })
      .catch(() => { /* use defaults */ });
  }, []);

  // Apply opacity to the actual window whenever it changes
  useEffect(() => {
    const opacity = OPACITY_LEVELS[opacityIndex] ?? 1;
    getCurrentWindow().setOpacity(opacity).catch(() => {});
  }, [opacityIndex]);

  // Blur-to-close: listen for focus changes to hide window when not pinned
  useEffect(() => {
    let unlisten: (() => void) | undefined;

    getCurrentWindow().onFocusChanged(({ payload: focused }) => {
      if (!focused && !pinnedRef.current) {
        getCurrentWindow().hide().catch(() => {});
      }
    }).then((cleanup) => {
      unlisten = cleanup;
    }).catch(() => {});

    return () => {
      unlisten?.();
    };
  }, []);

  const onPinToggle = (): void => {
    const next = !pinned;
    setPinned(next);
    pinnedRef.current = next;
    getCurrentWindow().setAlwaysOnTop(next).catch(() => {});
  };

  const onOpacityCycle = (): void => {
    setOpacityIndex((prev) => (prev + 1) % OPACITY_LEVELS.length);
  };

  const shellStyle: React.CSSProperties = {
    "--snapparse-font-size": `${fontSize}px`
  } as React.CSSProperties;

  return {
    pinned,
    fontSize,
    onPinToggle,
    onOpacityCycle,
    shellStyle
  };
}
