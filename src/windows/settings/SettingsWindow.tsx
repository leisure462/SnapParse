import { invoke } from "@tauri-apps/api/core";
import { useEffect, useMemo, useRef, useState } from "react";
import {
  defaultSettings,
  mergeSettings,
  validateSettings,
  type AppSettings,
  type ThemeMode
} from "../../shared/settings";
import { saveThemeMode } from "../theme/themeStore";
import ApiSettingsSection from "./sections/ApiSettings";
import ToolbarSettingsSection from "./sections/ToolbarSettings";
import WindowSettingsSection from "./sections/WindowSettings";
import FeatureSettingsSection from "./sections/FeatureSettings";
import AdvancedSettingsSection from "./sections/AdvancedSettings";
import "./settings.css";

type SectionKey = "api" | "toolbar" | "window" | "features" | "advanced";

const SECTIONS: Array<{ key: SectionKey; label: string }> = [
  { key: "api", label: "API配置" },
  { key: "toolbar", label: "工具栏" },
  { key: "window", label: "功能窗口" },
  { key: "features", label: "功能" },
  { key: "advanced", label: "高级设置" }
];

function persistThemeFromSettings(settings: AppSettings): void {
  saveThemeMode(settings.toolbar.themeMode as ThemeMode);
}

export default function SettingsWindow(): JSX.Element {
  const [activeSection, setActiveSection] = useState<SectionKey>("api");
  const [settings, setSettings] = useState<AppSettings>(() => defaultSettings());
  const [status, setStatus] = useState<"loading" | "saving" | "saved" | "error">("loading");
  const [statusText, setStatusText] = useState("正在加载设置...");
  const [isResettingDefaults, setIsResettingDefaults] = useState(false);
  const hasHydratedRef = useRef(false);
  const skipNextAutoSaveRef = useRef(true);
  const saveTimerRef = useRef<number | null>(null);
  const latestSaveJobRef = useRef(0);

  useEffect(() => {
    let cancelled = false;

    const load = async (): Promise<void> => {
      try {
        const value = await invoke<AppSettings>("get_settings");
        if (cancelled) {
          return;
        }

        const normalized = mergeSettings(value as Partial<AppSettings>);
        setSettings(normalized);
        persistThemeFromSettings(normalized);
        setStatus("saved");
        setStatusText("自动保存已开启");
      } catch {
        if (!cancelled) {
          const defaults = defaultSettings();
          setSettings(defaults);
          persistThemeFromSettings(defaults);
          setStatus("saved");
          setStatusText("已载入默认配置，自动保存已开启");
        }
      } finally {
        hasHydratedRef.current = true;
        skipNextAutoSaveRef.current = true;
      }
    };

    void load();

    return () => {
      cancelled = true;
    };
  }, []);

  const onSettingsChange = (next: AppSettings): void => {
    const normalized = mergeSettings(next);
    setSettings(normalized);

    if (normalized.toolbar.themeMode !== settings.toolbar.themeMode) {
      persistThemeFromSettings(normalized);
    }
  };

  useEffect(() => {
    if (!hasHydratedRef.current) {
      return;
    }

    if (skipNextAutoSaveRef.current) {
      skipNextAutoSaveRef.current = false;
      return;
    }

    if (saveTimerRef.current !== null) {
      window.clearTimeout(saveTimerRef.current);
    }

    setStatus("saving");
    setStatusText("自动保存中...");

    const saveJob = ++latestSaveJobRef.current;

    saveTimerRef.current = window.setTimeout(() => {
      void (async (): Promise<void> => {
        try {
          const validated = validateSettings(settings);
          await invoke("save_settings", { settings: validated });

          if (saveJob !== latestSaveJobRef.current) {
            return;
          }

          setStatus("saved");
          setStatusText("已自动保存");
        } catch (error) {
          if (saveJob !== latestSaveJobRef.current) {
            return;
          }

          const message = error instanceof Error ? error.message : String(error);
          setStatus("error");
          setStatusText(`保存失败：${message}`);
        }
      })();
    }, 260);

    return () => {
      if (saveTimerRef.current !== null) {
        window.clearTimeout(saveTimerRef.current);
        saveTimerRef.current = null;
      }
    };
  }, [settings]);

  const resetAllDefaults = async (): Promise<void> => {
    setIsResettingDefaults(true);
    try {
      const value = await invoke<AppSettings>("reset_settings");
      const normalized = validateSettings(value as Partial<AppSettings>);
      skipNextAutoSaveRef.current = true;
      setSettings(normalized);
      persistThemeFromSettings(normalized);
      setStatus("saved");
      setStatusText("已恢复全部默认设置");
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      setStatus("error");
      setStatusText(`恢复默认失败：${message}`);
    } finally {
      setIsResettingDefaults(false);
    }
  };

  const sectionPanel = useMemo(() => {
    if (activeSection === "api") {
      return <ApiSettingsSection settings={settings} onChange={onSettingsChange} />;
    }

    if (activeSection === "toolbar") {
      return <ToolbarSettingsSection settings={settings} onChange={onSettingsChange} />;
    }

    if (activeSection === "window") {
      return <WindowSettingsSection settings={settings} onChange={onSettingsChange} />;
    }

    if (activeSection === "features") {
      return <FeatureSettingsSection settings={settings} onChange={onSettingsChange} />;
    }

    return (
      <AdvancedSettingsSection
        settings={settings}
        onChange={onSettingsChange}
        onResetAllDefaults={() => {
          void resetAllDefaults();
        }}
        isResettingDefaults={isResettingDefaults}
      />
    );
  }, [activeSection, isResettingDefaults, onSettingsChange, settings]);

  return (
    <main className="settings-shell">
      <section className="settings-layout">
        <aside className="settings-sidebar" role="tablist" aria-label="设置分组">
          <header className="settings-sidebar-header">
            <h1 className="settings-brand">SnapParse</h1>
          </header>

          {SECTIONS.map((item) => (
            <button
              key={item.key}
              role="tab"
              aria-selected={activeSection === item.key}
              className="settings-tab"
              onClick={() => {
                setActiveSection(item.key);
              }}
            >
              {item.label}
            </button>
          ))}
        </aside>

        <section className="settings-main">
          <header className="settings-main-header">
            <div className={`settings-status ${status}`}>{statusText}</div>
          </header>

          <div className="settings-content">{sectionPanel}</div>
        </section>
      </section>
    </main>
  );
}
