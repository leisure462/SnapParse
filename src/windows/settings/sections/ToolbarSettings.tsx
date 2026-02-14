import type { AppSettings, TriggerMode } from "../../../shared/settings";
import type { SettingsSectionProps } from "./sectionTypes";

const TRIGGER_OPTIONS: Array<{ value: TriggerMode; label: string; tip: string }> = [
  {
    value: "selection",
    label: "划词",
    tip: "划词后立即显示工具栏"
  },
  {
    value: "ctrl",
    label: "Ctrl 键",
    tip: "划词后需再长按 Ctrl 键才显示工具栏"
  },
  {
    value: "hotkey",
    label: "快捷键",
    tip: "划词后按自定义快捷键显示工具栏"
  }
];

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

      <div className="settings-trigger-card settings-trigger-card-inline">
        <div className="settings-trigger-copy">
          <div className="settings-trigger-row">
            <span className="settings-trigger-title">取词方式</span>
            <span className="settings-trigger-help" aria-hidden="true">
              ?
            </span>
          </div>
          <p className="settings-hint">划词后，触发取词并显示工具栏的方式</p>
        </div>

        <div className="settings-trigger-segment-wrap settings-trigger-segment-wrap-right">
          <div className="settings-segmented" role="radiogroup" aria-label="取词方式">
            {TRIGGER_OPTIONS.map((item) => {
              const isActive = settings.toolbar.triggerMode === item.value;
              return (
                <button
                  key={item.value}
                  type="button"
                  role="radio"
                  aria-checked={isActive}
                  className={`settings-segment-btn ${isActive ? "active" : ""}`}
                  title={item.tip}
                  onClick={() => {
                    onChange(
                      patchToolbar(settings, (toolbar) => ({
                        ...toolbar,
                        triggerMode: item.value
                      }))
                    );
                  }}
                >
                  {item.label}
                </button>
              );
            })}
          </div>
        </div>
      </div>

      <label className="settings-switch settings-switch-full">
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
        <span>紧凑模式（仅显示图标）</span>
      </label>
    </section>
  );
}
