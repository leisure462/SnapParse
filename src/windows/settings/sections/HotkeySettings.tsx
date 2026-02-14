import type { KeyboardEvent } from "react";
import type { AppSettings } from "../../../shared/settings";
import type { SettingsSectionProps } from "./sectionTypes";

const DEFAULT_TRIGGER_HOTKEY = "Ctrl+Shift+Space";

function normalizeKey(key: string): string | null {
  if (key === " ") {
    return "Space";
  }

  if (key.length === 1) {
    return key.toUpperCase();
  }

  const keyMap: Record<string, string> = {
    Escape: "Esc",
    Enter: "Enter",
    Backspace: "Backspace",
    Delete: "Delete",
    ArrowUp: "Up",
    ArrowDown: "Down",
    ArrowLeft: "Left",
    ArrowRight: "Right"
  };

  if (keyMap[key]) {
    return keyMap[key];
  }

  if (/^F\d{1,2}$/.test(key)) {
    return key;
  }

  return null;
}

function formatHotkey(event: KeyboardEvent<HTMLInputElement>): string | null {
  const key = normalizeKey(event.key);
  if (!key) {
    return null;
  }

  const parts: string[] = [];
  if (event.ctrlKey) {
    parts.push("Ctrl");
  }
  if (event.shiftKey) {
    parts.push("Shift");
  }
  if (event.altKey) {
    parts.push("Alt");
  }
  if (event.metaKey) {
    parts.push("Meta");
  }

  if (["Ctrl", "Shift", "Alt", "Meta"].includes(key)) {
    return parts.length ? parts.join("+") : null;
  }

  return [...parts, key].join("+");
}

function patchToolbar(
  settings: AppSettings,
  updater: (toolbar: AppSettings["toolbar"]) => AppSettings["toolbar"]
): AppSettings {
  return {
    ...settings,
    toolbar: updater(settings.toolbar)
  };
}

export default function HotkeySettingsSection(props: SettingsSectionProps): JSX.Element {
  const { settings, onChange } = props;

  return (
    <section className="settings-section" aria-label="快捷键设置面板">
      <h2>快捷键设置</h2>
      <p className="settings-hint">自定义触发工具栏的快捷键组合。</p>

      <label className="settings-field">
        <span>触发快捷键</span>
        <input
          type="text"
          readOnly
          value={settings.toolbar.triggerHotkey}
          placeholder={DEFAULT_TRIGGER_HOTKEY}
          onKeyDown={(event) => {
            event.preventDefault();

            if (event.key === "Backspace" || event.key === "Delete") {
              onChange(
                patchToolbar(settings, (toolbar) => ({
                  ...toolbar,
                  triggerHotkey: DEFAULT_TRIGGER_HOTKEY
                }))
              );
              return;
            }

            const hotkey = formatHotkey(event);
            if (!hotkey) {
              return;
            }

            onChange(
              patchToolbar(settings, (toolbar) => ({
                ...toolbar,
                triggerHotkey: hotkey
              }))
            );
          }}
        />
      </label>

      <div className="settings-inline-actions">
        <span className="settings-hint">点击输入框后按下组合键即可修改。按 Delete 可恢复默认值。</span>
        <button
          type="button"
          className="settings-api-test-btn"
          onClick={() => {
            onChange(
              patchToolbar(settings, (toolbar) => ({
                ...toolbar,
                triggerHotkey: DEFAULT_TRIGGER_HOTKEY
              }))
            );
          }}
        >
          恢复默认快捷键
        </button>
      </div>

    </section>
  );
}
