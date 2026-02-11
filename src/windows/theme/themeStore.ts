import { useEffect, useMemo, useState } from "react";

export type ThemeMode = "light" | "dark" | "system";

const STORAGE_KEY = "snapparse.themeMode";
const THEME_EVENT = "snapparse-theme-changed";

function resolveSystemTheme(): Exclude<ThemeMode, "system"> {
  return window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
}

function normalizeThemeMode(value: string | null): ThemeMode {
  if (value === "light" || value === "dark" || value === "system") {
    return value;
  }

  return "dark";
}

export function getStoredThemeMode(): ThemeMode {
  return normalizeThemeMode(window.localStorage.getItem(STORAGE_KEY));
}

export function getEffectiveTheme(mode: ThemeMode): Exclude<ThemeMode, "system"> {
  return mode === "system" ? resolveSystemTheme() : mode;
}

export function applyThemeMode(mode: ThemeMode): void {
  const resolved = getEffectiveTheme(mode);
  document.documentElement.dataset.theme = resolved;
}

export function saveThemeMode(mode: ThemeMode): void {
  window.localStorage.setItem(STORAGE_KEY, mode);
  applyThemeMode(mode);
  window.dispatchEvent(new CustomEvent(THEME_EVENT, { detail: mode }));
}

export function initializeThemeMode(): ThemeMode {
  const mode = getStoredThemeMode();
  applyThemeMode(mode);
  return mode;
}

export function toggleThemeMode(current: ThemeMode): ThemeMode {
  if (current === "system") {
    return "light";
  }

  return current === "dark" ? "light" : "dark";
}

export function useThemeMode(): {
  mode: ThemeMode;
  effective: Exclude<ThemeMode, "system">;
  setMode: (mode: ThemeMode) => void;
  toggle: () => void;
} {
  const [mode, setModeState] = useState<ThemeMode>(() => initializeThemeMode());

  useEffect(() => {
    const onThemeChange = (event: Event): void => {
      const detail = (event as CustomEvent<ThemeMode>).detail;
      setModeState(normalizeThemeMode(detail));
    };

    window.addEventListener(THEME_EVENT, onThemeChange);

    const media = window.matchMedia("(prefers-color-scheme: dark)");
    const onMediaChange = (): void => {
      if (mode === "system") {
        applyThemeMode("system");
      }
    };

    media.addEventListener("change", onMediaChange);

    return () => {
      window.removeEventListener(THEME_EVENT, onThemeChange);
      media.removeEventListener("change", onMediaChange);
    };
  }, [mode]);

  const api = useMemo(
    () => ({
      mode,
      effective: getEffectiveTheme(mode),
      setMode: (next: ThemeMode): void => {
        saveThemeMode(next);
      },
      toggle: (): void => {
        saveThemeMode(toggleThemeMode(mode));
      }
    }),
    [mode]
  );

  return api;
}
