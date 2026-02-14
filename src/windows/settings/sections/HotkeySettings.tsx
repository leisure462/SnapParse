import type { KeyboardEvent } from "react";
import type { AppSettings } from "../../../shared/settings";
import type { SettingsSectionProps } from "./sectionTypes";

const DEFAULT_TRIGGER_HOTKEY = "Ctrl+Shift+Space";
const DEFAULT_SCREENSHOT_HOTKEY = "Ctrl+Shift+X";
const DEFAULT_QUICK_OCR_HOTKEY = "Alt+S";
const DEFAULT_REGION_MODE_HOTKEY = "Ctrl+R";
const DEFAULT_FULLSCREEN_MODE_HOTKEY = "Ctrl+A";
const DEFAULT_WINDOW_MODE_HOTKEY = "Ctrl+M";

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

      <label className="settings-field">
        <span>截屏快捷键</span>
        <input
          type="text"
          readOnly
          value={settings.ocr.captureHotkey}
          placeholder={DEFAULT_SCREENSHOT_HOTKEY}
          onKeyDown={(event) => {
            event.preventDefault();

            if (event.key === "Backspace" || event.key === "Delete") {
              onChange(
                patchOcr(settings, (ocr) => ({
                  ...ocr,
                  captureHotkey: DEFAULT_SCREENSHOT_HOTKEY
                }))
              );
              return;
            }

            const hotkey = formatHotkey(event);
            if (!hotkey) {
              return;
            }

            onChange(
              patchOcr(settings, (ocr) => ({
                ...ocr,
                captureHotkey: hotkey
              }))
            );
          }}
        />
      </label>

      <label className="settings-field">
        <span>OCR 快捷键</span>
        <input
          type="text"
          readOnly
          value={settings.ocr.quickOcrHotkey}
          placeholder={DEFAULT_QUICK_OCR_HOTKEY}
          onKeyDown={(event) => {
            event.preventDefault();

            if (event.key === "Backspace" || event.key === "Delete") {
              onChange(
                patchOcr(settings, (ocr) => ({
                  ...ocr,
                  quickOcrHotkey: DEFAULT_QUICK_OCR_HOTKEY
                }))
              );
              return;
            }

            const hotkey = formatHotkey(event);
            if (!hotkey) {
              return;
            }

            onChange(
              patchOcr(settings, (ocr) => ({
                ...ocr,
                quickOcrHotkey: hotkey
              }))
            );
          }}
        />
      </label>

      <div className="settings-grid-3">
        <label className="settings-field">
          <span>区域模式快捷键</span>
          <input
            type="text"
            readOnly
            value={settings.ocr.modeHotkeys.region}
            placeholder={DEFAULT_REGION_MODE_HOTKEY}
            onKeyDown={(event) => {
              event.preventDefault();

              if (event.key === "Backspace" || event.key === "Delete") {
                onChange(
                  patchOcr(settings, (ocr) => ({
                    ...ocr,
                    modeHotkeys: {
                      ...ocr.modeHotkeys,
                      region: DEFAULT_REGION_MODE_HOTKEY
                    }
                  }))
                );
                return;
              }

              const hotkey = formatHotkey(event);
              if (!hotkey) {
                return;
              }

              onChange(
                patchOcr(settings, (ocr) => ({
                  ...ocr,
                  modeHotkeys: {
                    ...ocr.modeHotkeys,
                    region: hotkey
                  }
                }))
              );
            }}
          />
        </label>

        <label className="settings-field">
          <span>全屏模式快捷键</span>
          <input
            type="text"
            readOnly
            value={settings.ocr.modeHotkeys.fullscreen}
            placeholder={DEFAULT_FULLSCREEN_MODE_HOTKEY}
            onKeyDown={(event) => {
              event.preventDefault();

              if (event.key === "Backspace" || event.key === "Delete") {
                onChange(
                  patchOcr(settings, (ocr) => ({
                    ...ocr,
                    modeHotkeys: {
                      ...ocr.modeHotkeys,
                      fullscreen: DEFAULT_FULLSCREEN_MODE_HOTKEY
                    }
                  }))
                );
                return;
              }

              const hotkey = formatHotkey(event);
              if (!hotkey) {
                return;
              }

              onChange(
                patchOcr(settings, (ocr) => ({
                  ...ocr,
                  modeHotkeys: {
                    ...ocr.modeHotkeys,
                    fullscreen: hotkey
                  }
                }))
              );
            }}
          />
        </label>

        <label className="settings-field">
          <span>窗口模式快捷键</span>
          <input
            type="text"
            readOnly
            value={settings.ocr.modeHotkeys.window}
            placeholder={DEFAULT_WINDOW_MODE_HOTKEY}
            onKeyDown={(event) => {
              event.preventDefault();

              if (event.key === "Backspace" || event.key === "Delete") {
                onChange(
                  patchOcr(settings, (ocr) => ({
                    ...ocr,
                    modeHotkeys: {
                      ...ocr.modeHotkeys,
                      window: DEFAULT_WINDOW_MODE_HOTKEY
                    }
                  }))
                );
                return;
              }

              const hotkey = formatHotkey(event);
              if (!hotkey) {
                return;
              }

              onChange(
                patchOcr(settings, (ocr) => ({
                  ...ocr,
                  modeHotkeys: {
                    ...ocr.modeHotkeys,
                    window: hotkey
                  }
                }))
              );
            }}
          />
        </label>
      </div>

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
        <button
          type="button"
          className="settings-api-test-btn"
          onClick={() => {
            onChange(
              patchOcr(settings, (ocr) => ({
                ...ocr,
                captureHotkey: DEFAULT_SCREENSHOT_HOTKEY,
                quickOcrHotkey: DEFAULT_QUICK_OCR_HOTKEY,
                modeHotkeys: {
                  region: DEFAULT_REGION_MODE_HOTKEY,
                  fullscreen: DEFAULT_FULLSCREEN_MODE_HOTKEY,
                  window: DEFAULT_WINDOW_MODE_HOTKEY
                }
              }))
            );
          }}
        >
          恢复截屏/OCR快捷键
        </button>
      </div>

    </section>
  );
}
