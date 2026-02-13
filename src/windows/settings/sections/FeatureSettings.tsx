import { useMemo, useState } from "react";
import type {
  AppSettings,
  BuiltinActionId,
  CustomFeatureAction
} from "../../../shared/settings";
import type { SettingsSectionProps } from "./sectionTypes";

const BUILTIN_LABEL: Record<BuiltinActionId, string> = {
  translate: "翻译",
  explain: "解释",
  summarize: "总结",
  optimize: "优化",
  search: "搜索",
  copy: "复制"
};

const ICON_PRESETS = [
  "sparkles", "rocket", "wand", "bolt", "atom", "beaker", "pen", "book", "target", "light",
  "leaf", "flame", "drop", "wave", "cloud", "sun", "moon", "star", "planet", "compass",
  "map", "code", "terminal", "chip", "cpu", "database", "folder", "file", "link", "globe",
  "search", "filter", "shield", "lock", "key", "camera", "image", "video", "music", "mic",
  "message", "chat", "mail", "calendar", "clock", "flag", "check", "heart", "gift", "tools"
] as const;

const DEFAULT_CUSTOM_PROMPT = "请根据以下内容完成任务：\n{{text}}";

function patchSettings(
  settings: AppSettings,
  updater: (next: AppSettings) => AppSettings
): AppSettings {
  return updater(settings);
}

function makeCustomAction(input: {
  name: string;
  icon: string;
  prompt: string;
  order: number;
}): CustomFeatureAction {
  const idSeed = input.name
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 32);

  const id = `custom-${idSeed || "action"}-${Date.now()}`;

  return {
    id,
    name: input.name.trim(),
    icon: input.icon.trim(),
    prompt: input.prompt.trim(),
    enabled: true,
    order: input.order
  };
}

export default function FeatureSettingsSection(props: SettingsSectionProps): JSX.Element {
  const { settings, onChange } = props;
  const [isDialogOpen, setDialogOpen] = useState(false);
  const [newName, setNewName] = useState("");
  const [newIcon, setNewIcon] = useState<string>(ICON_PRESETS[0]);
  const [newPrompt, setNewPrompt] = useState(DEFAULT_CUSTOM_PROMPT);

  const builtinActions = useMemo(() => {
    return [...settings.toolbar.actions].sort((a, b) => a.order - b.order);
  }, [settings.toolbar.actions]);

  const customActions = useMemo(() => {
    return [...settings.features.customActions].sort((a, b) => a.order - b.order);
  }, [settings.features.customActions]);

  const previewItems = useMemo(() => {
    const builtin = builtinActions
      .filter((item) => item.enabled)
      .map((item) => ({ id: item.id, label: BUILTIN_LABEL[item.id] }));

    const custom = settings.features.customActionsEnabled
      ? customActions.filter((item) => item.enabled).map((item) => ({ id: item.id, label: item.name }))
      : [];

    return [...builtin, ...custom];
  }, [builtinActions, customActions, settings.features.customActionsEnabled]);

  const resetDialog = (): void => {
    setNewName("");
    setNewIcon(ICON_PRESETS[0]);
    setNewPrompt(DEFAULT_CUSTOM_PROMPT);
    setDialogOpen(false);
  };

  return (
    <section className="settings-section" aria-label="功能配置面板">
      <h2>功能</h2>
      <p className="settings-hint">右侧开关控制功能是否出现在条形栏，顶部预览会实时更新。</p>

      <div className="settings-feature-preview">
        <div className="settings-feature-preview-title">条形栏预览</div>
        <div className="settings-feature-preview-bar" role="status" aria-live="polite">
          {previewItems.length === 0 ? <span className="settings-feature-empty">暂无启用功能</span> : null}
          {previewItems.map((item) => (
            <span key={item.id} className="settings-feature-pill">
              {item.label}
            </span>
          ))}
        </div>
      </div>

      <div className="settings-action-list">
        {builtinActions.map((action) => (
          <label key={action.id} className="settings-switch action-item">
            <input
              type="checkbox"
              checked={action.enabled}
              onChange={(event) => {
                const enabled = event.target.checked;
                const enabledActions = enabled
                  ? Array.from(new Set([...settings.features.enabledActions, action.id]))
                  : settings.features.enabledActions.filter((item) => item !== action.id);

                onChange(
                  patchSettings(settings, (current) => ({
                    ...current,
                    toolbar: {
                      ...current.toolbar,
                      actions: current.toolbar.actions.map((item) =>
                        item.id === action.id
                          ? {
                              ...item,
                              enabled
                            }
                          : item
                      )
                    },
                    features: {
                      ...current.features,
                      enabledActions
                    }
                  }))
                );
              }}
            />
            <span>{BUILTIN_LABEL[action.id]}</span>
          </label>
        ))}
      </div>

      <label className="settings-switch">
        <input
          type="checkbox"
          checked={settings.features.customActionsEnabled}
          onChange={(event) => {
            onChange(
              patchSettings(settings, (current) => ({
                ...current,
                features: {
                  ...current.features,
                  customActionsEnabled: event.target.checked
                }
              }))
            );
          }}
        />
        <span>启用自定义功能</span>
      </label>

      {settings.features.customActionsEnabled ? (
        <>
          <div className="settings-inline-actions">
            <span className="settings-hint">
              可添加自定义名称、图标和 Prompt，支持 <code>{"{{text}}"}</code> 占位符。
            </span>
            <button
              type="button"
              className="settings-api-test-btn"
              onClick={() => {
                setDialogOpen(true);
              }}
            >
              新增自定义功能
            </button>
          </div>

          <div className="settings-action-list">
            {customActions.map((action) => (
              <label key={action.id} className="settings-switch action-item">
                <input
                  type="checkbox"
                  checked={action.enabled}
                  onChange={(event) => {
                    const enabled = event.target.checked;
                    onChange(
                      patchSettings(settings, (current) => ({
                        ...current,
                        features: {
                          ...current.features,
                          customActions: current.features.customActions.map((item) =>
                            item.id === action.id
                              ? {
                                  ...item,
                                  enabled
                                }
                              : item
                          )
                        }
                      }))
                    );
                  }}
                />
                <span>
                  {action.name} · {action.icon}
                </span>
              </label>
            ))}
          </div>
        </>
      ) : null}

      {isDialogOpen ? (
        <div className="settings-dialog-mask" role="dialog" aria-modal="true" aria-label="新增自定义功能">
          <div className="settings-dialog-card">
            <h3>新增自定义功能</h3>

            <label className="settings-field">
              <span>功能名称</span>
              <input
                type="text"
                value={newName}
                onChange={(event) => {
                  setNewName(event.target.value);
                }}
                placeholder="例如：润色为商务语气"
              />
            </label>

            <label className="settings-field">
              <span>图标（50 预置）</span>
              <select
                value={newIcon}
                onChange={(event) => {
                  setNewIcon(event.target.value);
                }}
              >
                {ICON_PRESETS.map((item) => (
                  <option key={item} value={item}>
                    {item}
                  </option>
                ))}
              </select>
            </label>

            <label className="settings-field">
              <span>Prompt 模板</span>
              <textarea
                value={newPrompt}
                onChange={(event) => {
                  setNewPrompt(event.target.value);
                }}
              />
            </label>

            <div className="settings-inline-actions">
              <span className="settings-hint">
                占位符：<code>{"{{text}}"}</code>、<code>{"{{language}}"}</code>、<code>{"{{target_language}}"}</code>
              </span>
              <div className="settings-dialog-actions">
                <button
                  type="button"
                  className="settings-size-btn"
                  onClick={() => {
                    resetDialog();
                  }}
                >
                  取消
                </button>
                <button
                  type="button"
                  className="settings-api-test-btn"
                  disabled={!newName.trim() || !newPrompt.trim()}
                  onClick={() => {
                    const order = settings.features.customActions.length;
                    const nextAction = makeCustomAction({
                      name: newName,
                      icon: newIcon,
                      prompt: newPrompt,
                      order
                    });

                    onChange(
                      patchSettings(settings, (current) => ({
                        ...current,
                        features: {
                          ...current.features,
                          customActionsEnabled: true,
                          customActions: [...current.features.customActions, nextAction]
                        }
                      }))
                    );

                    resetDialog();
                  }}
                >
                  保存
                </button>
              </div>
            </div>
          </div>
        </div>
      ) : null}
    </section>
  );
}
