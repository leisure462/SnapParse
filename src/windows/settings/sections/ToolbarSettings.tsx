import type { AppSettings, ThemeMode, TriggerMode } from "../../../shared/settings";
import type { SettingsSectionProps } from "./sectionTypes";

const TRIGGER_OPTIONS: TriggerMode[] = ["selection", "ctrl", "hotkey"];
const THEME_OPTIONS: ThemeMode[] = ["light", "dark", "system"];

function patchToolbar(
  settings: AppSettings,
  updater: (toolbar: AppSettings["toolbar"]) => AppSettings["toolbar"]
): AppSettings {
  return {
    ...settings,
    toolbar: updater(settings.toolbar)
  };
}

export default function ToolbarSettingsSection(props: SettingsSectionProps): JSX.Element {
  const { settings, onChange } = props;

  return (
    <section className="settings-section" aria-label="工具栏配置面板">
      <h2>工具栏</h2>
      <p className="settings-hint">控制划词后工具栏行为和外观</p>

      <label className="settings-field">
        <span>触发方式</span>
        <select
          value={settings.toolbar.triggerMode}
          onChange={(event) => {
            const next = event.target.value as TriggerMode;
            onChange(
              patchToolbar(settings, (toolbar) => ({
                ...toolbar,
                triggerMode: next
              }))
            );
          }}
        >
          {TRIGGER_OPTIONS.map((item) => (
            <option key={item} value={item}>
              {item}
            </option>
          ))}
        </select>
      </label>

      <div className="settings-grid-2">
        <label className="settings-switch">
          <input
            type="checkbox"
            checked={settings.toolbar.compactMode}
            onChange={(event) => {
              onChange(
                patchToolbar(settings, (toolbar) => ({
                  ...toolbar,
                  compactMode: event.target.checked
                }))
              );
            }}
          />
          <span>紧凑模式</span>
        </label>

        <label className="settings-switch">
          <input
            type="checkbox"
            checked={settings.toolbar.showLabel}
            onChange={(event) => {
              onChange(
                patchToolbar(settings, (toolbar) => ({
                  ...toolbar,
                  showLabel: event.target.checked
                }))
              );
            }}
          />
          <span>显示文字标签</span>
        </label>
      </div>

      <label className="settings-field">
        <span>默认主题模式</span>
        <select
          value={settings.toolbar.themeMode}
          onChange={(event) => {
            const next = event.target.value as ThemeMode;
            onChange(
              patchToolbar(settings, (toolbar) => ({
                ...toolbar,
                themeMode: next
              }))
            );
          }}
        >
          {THEME_OPTIONS.map((item) => (
            <option key={item} value={item}>
              {item}
            </option>
          ))}
        </select>
      </label>

    </section>
  );
}
