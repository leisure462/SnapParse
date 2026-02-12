import type { AppSettings, WindowSizePreset } from "../../../shared/settings";
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

const SIZE_PRESETS: Array<{ value: WindowSizePreset; label: string; desc: string }> = [
  { value: "large", label: "大", desc: "680 x 520" },
  { value: "medium", label: "中", desc: "520 x 400" },
  { value: "small", label: "小", desc: "400 x 320" }
];

export default function WindowSettingsSection(props: SettingsSectionProps): JSX.Element {
  const { settings, onChange } = props;

  return (
    <section className="settings-section" aria-label="功能窗口配置面板">
      <h2>功能窗口</h2>

      <div className="settings-field">
        <span>窗口大小</span>
        <div className="settings-size-presets">
          {SIZE_PRESETS.map((preset) => (
            <button
              key={preset.value}
              type="button"
              className={`settings-size-btn ${settings.window.windowSize === preset.value ? "active" : ""}`}
              onClick={() => {
                onChange(
                  patchWindow(settings, (ws) => ({
                    ...ws,
                    windowSize: preset.value
                  }))
                );
              }}
            >
              <span className="settings-size-label">{preset.label}</span>
              <span className="settings-size-desc">{preset.desc}</span>
            </button>
          ))}
        </div>
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

    </section>
  );
}
