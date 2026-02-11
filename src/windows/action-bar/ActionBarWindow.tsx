import { invoke } from "@tauri-apps/api/core";
import { emit, listen } from "@tauri-apps/api/event";
import { useEffect, useState } from "react";
import { DEFAULT_ACTIONS, type ActionBarAction, type ActionBarActionId } from "./actions";
import "./actionBar.css";
import { toggleThemeMode, useThemeMode, type ThemeMode } from "../theme/themeStore";
import { defaultSettings, validateSettings, type AppSettings } from "../../shared/settings";

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

export default function ActionBarWindow(): JSX.Element {
  const [selectedText, setSelectedText] = useState("");
  const [isBusy, setBusy] = useState(false);
  const [showThemeToggle, setShowThemeToggle] = useState(true);
  const theme = useThemeMode();

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
        setShowThemeToggle(normalized.toolbar.showThemeToggleInToolbar);
        theme.setMode(normalized.toolbar.themeMode as ThemeMode);
      } catch {
        if (cancelled) {
          return;
        }

        const defaults = defaultSettings();
        setShowThemeToggle(defaults.toolbar.showThemeToggleInToolbar);
      }
    };

    void loadSettings();

    return () => {
      cancelled = true;
    };
  }, []);

  const persistThemeModeToSettings = async (mode: ThemeMode): Promise<void> => {
    try {
      const loaded = await invoke<AppSettings>("get_settings");
      const normalized = validateSettings(loaded as Partial<AppSettings>);
      await invoke("save_settings", {
        settings: {
          ...normalized,
          toolbar: {
            ...normalized.toolbar,
            themeMode: mode
          }
        }
      });
    } catch {
      // keep local fallback when settings command is unavailable
    }
  };

  const runAction = async (action: ActionBarAction): Promise<void> => {
    if (isBusy) {
      return;
    }

    setBusy(true);

    try {
      if (action.commandWindow) {
        await invoke("open_window", { kind: action.commandWindow });
        await emit("change-text", { text: selectedText, source: "action-bar" });
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

  const isDark = theme.effective === "dark";

  return (
    <section className="md2-action-bar" role="toolbar" aria-label="划词工具栏">
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

      {showThemeToggle ? (
        <button
          type="button"
          role="switch"
          aria-checked={isDark}
          aria-label="明暗切换"
          className="md2-theme-switch"
          onClick={() => {
            const next = toggleThemeMode(theme.mode);
            theme.setMode(next);
            void persistThemeModeToSettings(next);
          }}
        >
          <span className="md2-theme-thumb" />
          <span className="md2-theme-text">{isDark ? "暗" : "明"}</span>
        </button>
      ) : null}
    </section>
  );
}
