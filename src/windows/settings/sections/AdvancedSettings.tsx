import type { AppFilterMode, AppSettings, LogLevel } from "../../../shared/settings";
import type { SettingsSectionProps } from "./sectionTypes";

const APP_FILTER_MODES: AppFilterMode[] = ["off", "whitelist", "blacklist"];
const LOG_LEVELS: LogLevel[] = ["error", "warn", "info", "debug"];

function patchAdvanced(
  settings: AppSettings,
  updater: (advanced: AppSettings["advanced"]) => AppSettings["advanced"]
): AppSettings {
  return {
    ...settings,
    advanced: updater(settings.advanced)
  };
}

export default function AdvancedSettingsSection(props: SettingsSectionProps): JSX.Element {
  const { settings, onChange } = props;

  return (
    <section className="settings-section" aria-label="高级设置面板">
      <h2>高级设置</h2>

      <label className="settings-field">
        <span>应用筛选模式</span>
        <select
          value={settings.advanced.appFilterMode}
          onChange={(event) => {
            const mode = event.target.value as AppFilterMode;
            onChange(
              patchAdvanced(settings, (advanced) => ({
                ...advanced,
                appFilterMode: mode
              }))
            );
          }}
        >
          {APP_FILTER_MODES.map((mode) => (
            <option key={mode} value={mode}>
              {mode}
            </option>
          ))}
        </select>
      </label>

      <label className="settings-field">
        <span>日志级别</span>
        <select
          value={settings.advanced.logLevel}
          onChange={(event) => {
            const level = event.target.value as LogLevel;
            onChange(
              patchAdvanced(settings, (advanced) => ({
                ...advanced,
                logLevel: level
              }))
            );
          }}
        >
          {LOG_LEVELS.map((level) => (
            <option key={level} value={level}>
              {level}
            </option>
          ))}
        </select>
      </label>

      <label className="settings-field">
        <span>应用名单（每行一个进程名）</span>
        <textarea
          value={settings.advanced.appList.join("\n")}
          onChange={(event) => {
            const list = event.target.value
              .split(/\r?\n/)
              .map((item) => item.trim())
              .filter(Boolean);

            onChange(
              patchAdvanced(settings, (advanced) => ({
                ...advanced,
                appList: list
              }))
            );
          }}
        />
      </label>
    </section>
  );
}
