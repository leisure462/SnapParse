import type { AppSettings } from "../../../shared/settings";
import type { SettingsSectionProps } from "./sectionTypes";

function patchWindow(
  settings: AppSettings,
  updater: (windowSettings: AppSettings["window"]) => AppSettings["window"]
): AppSettings {
  return {
    ...settings,
    window: updater(settings.window)
  };
}

export default function WindowSettingsSection(props: SettingsSectionProps): JSX.Element {
  const { settings, onChange } = props;

  return (
    <section className="settings-section" aria-label="功能窗口配置面板">
      <h2>功能窗口</h2>

      <div className="settings-grid-2">
        <label className="settings-switch">
          <input
            type="checkbox"
            checked={settings.window.followToolbar}
            onChange={(event) => {
              onChange(
                patchWindow(settings, (windowSettings) => ({
                  ...windowSettings,
                  followToolbar: event.target.checked
                }))
              );
            }}
          />
          <span>跟随工具栏</span>
        </label>

        <label className="settings-switch">
          <input
            type="checkbox"
            checked={settings.window.rememberSize}
            onChange={(event) => {
              onChange(
                patchWindow(settings, (windowSettings) => ({
                  ...windowSettings,
                  rememberSize: event.target.checked
                }))
              );
            }}
          />
          <span>记住大小</span>
        </label>

        <label className="settings-switch">
          <input
            type="checkbox"
            checked={settings.window.autoClose}
            onChange={(event) => {
              onChange(
                patchWindow(settings, (windowSettings) => ({
                  ...windowSettings,
                  autoClose: event.target.checked
                }))
              );
            }}
          />
          <span>自动关闭</span>
        </label>

        <label className="settings-switch">
          <input
            type="checkbox"
            checked={settings.window.autoPin}
            onChange={(event) => {
              onChange(
                patchWindow(settings, (windowSettings) => ({
                  ...windowSettings,
                  autoPin: event.target.checked
                }))
              );
            }}
          />
          <span>自动置顶</span>
        </label>
      </div>

      <label className="settings-field">
        <span>透明度: {Math.round(settings.window.opacity * 100)}%</span>
        <input
          type="range"
          min={0.2}
          max={1}
          step={0.05}
          value={settings.window.opacity}
          onChange={(event) => {
            onChange(
              patchWindow(settings, (windowSettings) => ({
                ...windowSettings,
                opacity: Number(event.target.value)
              }))
            );
          }}
        />
      </label>
    </section>
  );
}
