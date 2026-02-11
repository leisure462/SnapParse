import { invoke } from "@tauri-apps/api/core";
import { useEffect, useMemo, useState } from "react";
import {
  defaultSettings,
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
  const [status, setStatus] = useState<"idle" | "saving" | "saved" | "error">("idle");
  const [statusText, setStatusText] = useState("");

  useEffect(() => {
    let cancelled = false;

    const load = async (): Promise<void> => {
      try {
        const value = await invoke<AppSettings>("get_settings");
        if (cancelled) {
          return;
        }

        const normalized = validateSettings(value as Partial<AppSettings>);
        setSettings(normalized);
        persistThemeFromSettings(normalized);
      } catch {
        if (!cancelled) {
          const defaults = defaultSettings();
          setSettings(defaults);
          persistThemeFromSettings(defaults);
        }
      }
    };

    void load();

    return () => {
      cancelled = true;
    };
  }, []);

  const onSettingsChange = (next: AppSettings): void => {
    const normalized = validateSettings(next);
    setSettings(normalized);

    if (normalized.toolbar.themeMode !== settings.toolbar.themeMode) {
      persistThemeFromSettings(normalized);
    }

    setStatus("idle");
    setStatusText("");
  };

  const save = async (): Promise<void> => {
    setStatus("saving");
    setStatusText("保存中...");

    try {
      await invoke("save_settings", { settings });
      setStatus("saved");
      setStatusText("配置已保存");
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      setStatus("error");
      setStatusText(`保存失败：${message}`);
    }
  };

  const reset = async (): Promise<void> => {
    try {
      const value = await invoke<AppSettings>("reset_settings");
      const normalized = validateSettings(value as Partial<AppSettings>);
      setSettings(normalized);
      persistThemeFromSettings(normalized);
      setStatus("saved");
      setStatusText("已恢复默认配置");
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      setStatus("error");
      setStatusText(`重置失败：${message}`);
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

    return <AdvancedSettingsSection settings={settings} onChange={onSettingsChange} />;
  }, [activeSection, onSettingsChange, settings]);

  return (
    <main className="settings-shell">
      <section className="settings-layout">
        <aside className="settings-sidebar" role="tablist" aria-label="设置分组">
          <header className="settings-sidebar-header">
            <h1>SnapParse 设置</h1>
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
            <div className={`settings-status ${status}`}>{statusText || "未保存更改"}</div>

            <div className="settings-actions">
              <button
                type="button"
                className="ghost"
                onClick={() => {
                  void reset();
                }}
              >
                恢复默认
              </button>
              <button
                type="button"
                className="primary"
                onClick={() => {
                  void save();
                }}
              >
                保存
              </button>
            </div>
          </header>

          <div className="settings-content">{sectionPanel}</div>
        </section>
      </section>
    </main>
  );
}
