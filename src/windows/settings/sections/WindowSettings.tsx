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
        <label className="settings-field">
          <span>窗口宽度: {settings.window.windowWidth}px</span>
          <input
            type="range"
            min={320}
            max={1600}
            step={10}
            value={settings.window.windowWidth}
            onChange={(event) => {
              onChange(
                patchWindow(settings, (windowSettings) => ({
                  ...windowSettings,
                  windowWidth: Number(event.target.value)
                }))
              );
            }}
          />
        </label>

        <label className="settings-field">
          <span>窗口高度: {settings.window.windowHeight}px</span>
          <input
            type="range"
            min={280}
            max={1200}
            step={10}
            value={settings.window.windowHeight}
            onChange={(event) => {
              onChange(
                patchWindow(settings, (windowSettings) => ({
                  ...windowSettings,
                  windowHeight: Number(event.target.value)
                }))
              );
            }}
          />
        </label>
      </div>

      <label className="settings-field">
        <span>字体大小: {settings.window.fontSize}px</span>
        <input
          type="range"
          min={10}
          max={24}
          step={1}
          value={settings.window.fontSize}
          onChange={(event) => {
            onChange(
              patchWindow(settings, (windowSettings) => ({
                ...windowSettings,
                fontSize: Number(event.target.value)
              }))
            );
          }}
        />
      </label>

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
