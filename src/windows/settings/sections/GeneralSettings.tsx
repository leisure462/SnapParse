import type { AppLanguage, AppSettings, ThemeMode } from "../../../shared/settings";
import type { SettingsSectionProps } from "./sectionTypes";

const THEME_OPTIONS: Array<{ value: ThemeMode; label: string }> = [
  { value: "light", label: "浅色" },
  { value: "dark", label: "暗色" },
  { value: "system", label: "跟随系统" }
];

const LANGUAGE_OPTIONS: Array<{ value: AppLanguage; label: string }> = [
  { value: "zh-CN", label: "简体中文（预置）" },
  { value: "en-US", label: "English (preset)" }
];

function patchGeneral(
  settings: AppSettings,
  updater: (general: AppSettings["general"]) => AppSettings["general"]
): AppSettings {
  return {
    ...settings,
    general: updater(settings.general)
  };
}

function patchThemeMode(settings: AppSettings, mode: ThemeMode): AppSettings {
  return {
    ...settings,
    toolbar: {
      ...settings.toolbar,
      themeMode: mode
    }
  };
}

export default function GeneralSettingsSection(props: SettingsSectionProps): JSX.Element {
  const { settings, onChange } = props;

  return (
    <section className="settings-section" aria-label="通用设置面板">
      <h2>通用设置</h2>
      <p className="settings-hint">应用启动行为、主题和语言偏好。</p>

      <label className="settings-field">
        <span>界面主题</span>
        <select
          value={settings.toolbar.themeMode}
          onChange={(event) => {
            const next = event.target.value as ThemeMode;
            onChange(patchThemeMode(settings, next));
          }}
        >
          {THEME_OPTIONS.map((item) => (
            <option key={item.value} value={item.value}>
              {item.label}
            </option>
          ))}
        </select>
      </label>

      <label className="settings-switch">
        <input
          type="checkbox"
          checked={settings.general.launchAtStartup}
          onChange={(event) => {
            onChange(
              patchGeneral(settings, (general) => ({
                ...general,
                launchAtStartup: event.target.checked
              }))
            );
          }}
        />
        <span>开机自启动（开机启动默认不弹设置窗口）</span>
      </label>

      <label className="settings-switch">
        <input
          type="checkbox"
          checked={settings.general.silentStartup}
          onChange={(event) => {
            onChange(
              patchGeneral(settings, (general) => ({
                ...general,
                silentStartup: event.target.checked
              }))
            );
          }}
        />
        <span>静默启动（手动启动时不弹出设置窗口）</span>
      </label>

      <label className="settings-field">
        <span>界面语言（预置）</span>
        <select
          value={settings.general.language}
          onChange={(event) => {
            const next = event.target.value as AppLanguage;
            onChange(
              patchGeneral(settings, (general) => ({
                ...general,
                language: next
              }))
            );
          }}
        >
          {LANGUAGE_OPTIONS.map((item) => (
            <option key={item.value} value={item.value}>
              {item.label}
            </option>
          ))}
        </select>
      </label>
      <p className="settings-hint">语言选项目前仅做预置存档，后续版本接入完整多语言界面。</p>
    </section>
  );
}
