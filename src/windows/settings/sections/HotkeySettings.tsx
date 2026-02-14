import type { KeyboardEvent } from "react";
import type { AppSettings } from "../../../shared/settings";
import type { SettingsSectionProps } from "./sectionTypes";

const DEFAULT_TRIGGER_HOTKEY = "Ctrl+Shift+Space";
const DEFAULT_SCREENSHOT_HOTKEY = "Ctrl+Shift+X";
const DEFAULT_QUICK_OCR_HOTKEY = "Ctrl+Shift+O";
const DEFAULT_FULLSCREEN_MODE_HOTKEY = "Ctrl+Shift+A";
const DEFAULT_WINDOW_MODE_HOTKEY = "Ctrl+Shift+M";

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

function patchOcr(
  settings: AppSettings,
  updater: (ocr: AppSettings["ocr"]) => AppSettings["ocr"]
): AppSettings {
  return {
    ...settings,
    ocr: updater(settings.ocr)
  };
}

function bindHotkeyInput(
  onChange: (hotkey: string) => void
): (event: KeyboardEvent<HTMLInputElement>) => void {
  return (event) => {
    event.preventDefault();
    const hotkey = formatHotkey(event);
    if (!hotkey) {
      return;
    }
    onChange(hotkey);
  };
}

export default function HotkeySettingsSection(props: SettingsSectionProps): JSX.Element {
  const { settings, onChange } = props;

  return (
    <section className="settings-section" aria-label="快捷键设置面板">
      <h2>快捷键设置</h2>
      <p className="settings-hint">快捷键统一管理。点击输入框后按组合键即可修改。</p>

      <div className="settings-grid-2">
        <label className="settings-field">
          <span>触发快捷键</span>
          <input
            type="text"
            readOnly
            value={settings.toolbar.triggerHotkey}
            placeholder={DEFAULT_TRIGGER_HOTKEY}
            onKeyDown={bindHotkeyInput((hotkey) => {
              onChange(
                patchToolbar(settings, (toolbar) => ({
                  ...toolbar,
                  triggerHotkey: hotkey
                }))
              );
            })}
          />
        </label>

        <label className="settings-field">
          <span>截屏快捷键（区域）</span>
          <input
            type="text"
            readOnly
            value={settings.ocr.captureHotkey}
            placeholder={DEFAULT_SCREENSHOT_HOTKEY}
            onKeyDown={bindHotkeyInput((hotkey) => {
              onChange(
                patchOcr(settings, (ocr) => ({
                  ...ocr,
                  captureHotkey: hotkey,
                  modeHotkeys: {
                    ...ocr.modeHotkeys,
                    region: hotkey
                  }
                }))
              );
            })}
          />
        </label>

        <label className="settings-field">
          <span>OCR 快捷键</span>
          <input
            type="text"
            readOnly
            value={settings.ocr.quickOcrHotkey}
            placeholder={DEFAULT_QUICK_OCR_HOTKEY}
            onKeyDown={bindHotkeyInput((hotkey) => {
              onChange(
                patchOcr(settings, (ocr) => ({
                  ...ocr,
                  quickOcrHotkey: hotkey
                }))
              );
            })}
          />
        </label>

        <label className="settings-field">
          <span>全屏模式快捷键</span>
          <input
            type="text"
            readOnly
            value={settings.ocr.modeHotkeys.fullscreen}
            placeholder={DEFAULT_FULLSCREEN_MODE_HOTKEY}
            onKeyDown={bindHotkeyInput((hotkey) => {
              onChange(
                patchOcr(settings, (ocr) => ({
                  ...ocr,
                  modeHotkeys: {
                    ...ocr.modeHotkeys,
                    fullscreen: hotkey
                  }
                }))
              );
            })}
          />
        </label>

        <label className="settings-field">
          <span>窗口模式快捷键</span>
          <input
            type="text"
            readOnly
            value={settings.ocr.modeHotkeys.window}
            placeholder={DEFAULT_WINDOW_MODE_HOTKEY}
            onKeyDown={bindHotkeyInput((hotkey) => {
              onChange(
                patchOcr(settings, (ocr) => ({
                  ...ocr,
                  modeHotkeys: {
                    ...ocr.modeHotkeys,
                    window: hotkey
                  }
                }))
              );
            })}
          />
        </label>

      </div>
    </section>
  );
}
