import type { AppSettings, CaptureMode } from "../../../shared/settings";
import type { SettingsSectionProps } from "./sectionTypes";

function patchOcr(
  settings: AppSettings,
  updater: (ocr: AppSettings["ocr"]) => AppSettings["ocr"]
): AppSettings {
  return {
    ...settings,
    ocr: updater(settings.ocr)
  };
}

const MODES: Array<{ value: CaptureMode; label: string; description: string }> = [
  { value: "region", label: "滑动截屏", description: "拖拽选区后截取" },
  { value: "fullscreen", label: "全屏截屏", description: "直接截取当前屏幕" },
  { value: "window", label: "窗口截屏", description: "悬停高亮并点击窗口" }
];

export default function ScreenshotSettingsSection(props: SettingsSectionProps): JSX.Element {
  const { settings, onChange } = props;

  return (
    <section className="settings-section" aria-label="截屏设置面板">
      <h2>截屏设置</h2>
      <p className="settings-hint">进入截屏遮罩后，可通过快捷键在滑动/全屏/窗口三种模式中切换。</p>

      <div className="settings-field">
        <span>默认截屏模式</span>
        <div className="settings-size-presets">
          {MODES.map((item) => (
            <button
              key={item.value}
              type="button"
              className={`settings-size-btn ${settings.ocr.captureDefaultMode === item.value ? "active" : ""}`}
              onClick={() => {
                onChange(
                  patchOcr(settings, (ocr) => ({
                    ...ocr,
                    captureDefaultMode: item.value
                  }))
                );
              }}
            >
              <span className="settings-size-label">{item.label}</span>
              <span className="settings-size-desc">{item.description}</span>
            </button>
          ))}
        </div>
      </div>

      <label className="settings-switch settings-switch-full">
        <input
          type="checkbox"
          checked={settings.ocr.showShortcutHints}
          onChange={(event) => {
            onChange(
              patchOcr(settings, (ocr) => ({
                ...ocr,
                showShortcutHints: event.target.checked
              }))
            );
          }}
        />
        <span>显示左下角快捷键提示</span>
      </label>

      <p className="settings-hint">截屏快捷键请在「快捷键设置」统一管理。</p>
    </section>
  );
}
