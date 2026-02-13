import { useMemo, useState } from "react";
import type {
  AppSettings,
  BuiltinActionId,
  CustomFeatureAction
} from "../../../shared/settings";
import {
  MAX_CUSTOM_ACTION_COUNT,
  MAX_CUSTOM_ACTION_NAME_LENGTH
} from "../../../shared/settings";
import {
  CUSTOM_ACTION_ICON_PRESETS,
  renderActionIcon,
  resolveCustomIconLabel
} from "../../common/actionIcon";
import type { SettingsSectionProps } from "./sectionTypes";

const BUILTIN_LABEL: Record<BuiltinActionId, string> = {
  translate: "翻译",
  explain: "解释",
  summarize: "总结",
  optimize: "优化",
  search: "搜索",
  copy: "复制"
};

const DEFAULT_CUSTOM_PROMPT = "";

const PLACEHOLDER_ITEMS = [
  {
    token: "{{text}}",
    hint: "选中的原文文本"
  },
  {
    token: "{{target_language}}",
    hint: "目标语言（如 zh-CN / en-US）"
  }
] as const;

function patchSettings(
  settings: AppSettings,
  updater: (next: AppSettings) => AppSettings
): AppSettings {
  return updater(settings);
}

function countChars(value: string): number {
  return Array.from(value).length;
}

function clampName(value: string): string {
  return Array.from(value).slice(0, MAX_CUSTOM_ACTION_NAME_LENGTH).join("");
}

function normalizeCustomOrder(actions: CustomFeatureAction[]): CustomFeatureAction[] {
  return actions
    .slice()
    .sort((a, b) => a.order - b.order)
    .map((item, index) => ({
      ...item,
      order: index
    }));
}

function makeCustomAction(input: {
  name: string;
  icon: string;
  prompt: string;
  model: string;
  order: number;
}): CustomFeatureAction {
  const safeName = clampName(input.name.trim());
  const idSeed = safeName
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 32);

  const id = `custom-${idSeed || "agent"}-${Date.now()}`;

  return {
    id,
    name: safeName,
    icon: input.icon.trim(),
    prompt: input.prompt.trim(),
    model: input.model.trim(),
    enabled: true,
    order: input.order
  };
}

export default function FeatureSettingsSection(props: SettingsSectionProps): JSX.Element {
  const { settings, onChange } = props;
  const [isDialogOpen, setDialogOpen] = useState(false);
  const [editingId, setEditingId] = useState<string | null>(null);
  const [newName, setNewName] = useState("");
  const [newIcon, setNewIcon] = useState<string>(CUSTOM_ACTION_ICON_PRESETS[0]?.id ?? "bot");
  const [newPrompt, setNewPrompt] = useState(DEFAULT_CUSTOM_PROMPT);
  const [newModel, setNewModel] = useState("");
  const [copiedToken, setCopiedToken] = useState<string | null>(null);

  const builtinActions = useMemo(() => {
    return [...settings.toolbar.actions].sort((a, b) => a.order - b.order);
  }, [settings.toolbar.actions]);

  const customActions = useMemo(() => {
    return [...settings.features.customActions].sort((a, b) => a.order - b.order);
  }, [settings.features.customActions]);

  const previewItems = useMemo(() => {
    const builtin = builtinActions
      .filter((item) => item.enabled)
      .map((item) => ({ id: item.id, label: BUILTIN_LABEL[item.id], icon: item.id }));

    const custom = customActions
      .filter((item) => item.enabled)
      .map((item) => ({ id: item.id, label: item.name, icon: item.icon }));

    return [...builtin, ...custom];
  }, [builtinActions, customActions]);

  const reachedCreateLimit = editingId === null && customActions.length >= MAX_CUSTOM_ACTION_COUNT;
  const nameCharCount = countChars(newName.trim());

  const closeDialog = (): void => {
    setDialogOpen(false);
    setEditingId(null);
    setNewName("");
    setNewModel("");
    setNewIcon(CUSTOM_ACTION_ICON_PRESETS[0]?.id ?? "bot");
    setNewPrompt(DEFAULT_CUSTOM_PROMPT);
    setCopiedToken(null);
  };

  const openCreateDialog = (): void => {
    if (customActions.length >= MAX_CUSTOM_ACTION_COUNT) {
      return;
    }

    setEditingId(null);
    setNewName("");
    setNewModel("");
    setNewIcon(CUSTOM_ACTION_ICON_PRESETS[0]?.id ?? "bot");
    setNewPrompt(DEFAULT_CUSTOM_PROMPT);
    setDialogOpen(true);
  };

  const openEditDialog = (action: CustomFeatureAction): void => {
    setEditingId(action.id);
    setNewName(clampName(action.name));
    setNewModel(action.model ?? "");
    setNewIcon(action.icon);
    setNewPrompt(action.prompt);
    setCopiedToken(null);
    setDialogOpen(true);
  };

  const copyPlaceholderToken = async (token: string): Promise<void> => {
    try {
      await navigator.clipboard.writeText(token);
    } catch {
      const fallback = document.createElement("textarea");
      fallback.value = token;
      fallback.style.position = "fixed";
      fallback.style.opacity = "0";
      document.body.append(fallback);
      fallback.select();
      document.execCommand("copy");
      fallback.remove();
    }

    setCopiedToken(token);
    window.setTimeout(() => {
      setCopiedToken((current) => (current === token ? null : current));
    }, 1200);
  };

  const saveDialog = (): void => {
    const safeName = clampName(newName).trim();
    const safePrompt = newPrompt.trim();

    if (!safeName || !safePrompt) {
      return;
    }

    if (!editingId && customActions.length >= MAX_CUSTOM_ACTION_COUNT) {
      return;
    }

    onChange(
      patchSettings(settings, (current) => {
        if (editingId) {
          return {
            ...current,
            features: {
              ...current.features,
              customActions: normalizeCustomOrder(
                current.features.customActions.map((item) =>
                  item.id === editingId
                    ? {
                        ...item,
                        name: safeName,
                        icon: newIcon,
                        prompt: safePrompt,
                        model: newModel.trim()
                      }
                    : item
                )
              )
            }
          };
        }

        const nextAction = makeCustomAction({
          name: safeName,
          icon: newIcon,
          prompt: safePrompt,
          model: newModel,
          order: current.features.customActions.length
        });

        return {
          ...current,
          features: {
            ...current.features,
            customActions: normalizeCustomOrder([...current.features.customActions, nextAction])
          }
        };
      })
    );

    closeDialog();
  };

  const deleteEditingAction = (): void => {
    if (!editingId) {
      return;
    }

    onChange(
      patchSettings(settings, (current) => ({
        ...current,
        features: {
          ...current.features,
          customActions: normalizeCustomOrder(current.features.customActions.filter((item) => item.id !== editingId))
        }
      }))
    );

    closeDialog();
  };

  const saveDisabled = !newName.trim() || !newPrompt.trim() || nameCharCount > MAX_CUSTOM_ACTION_NAME_LENGTH || reachedCreateLimit;

  return (
    <section className="settings-section" aria-label="功能配置面板">
      <div className="settings-section-topbar">
        <div>
          <h2>功能</h2>
          <p className="settings-hint">右侧开关控制功能是否出现在条形栏，顶部预览会实时更新。</p>
        </div>

        <button
          type="button"
          className="settings-api-test-btn"
          disabled={customActions.length >= MAX_CUSTOM_ACTION_COUNT}
          onClick={() => {
            openCreateDialog();
          }}
        >
          自定义agent {`(${customActions.length}/${MAX_CUSTOM_ACTION_COUNT})`}
        </button>
      </div>

      <div className="settings-feature-preview">
        <div className="settings-feature-preview-title">条形栏预览</div>
        <div className="settings-feature-preview-bar" role="status" aria-live="polite">
          {previewItems.length === 0 ? <span className="settings-feature-empty">暂无启用功能</span> : null}
          {previewItems.map((item) => (
            <span key={item.id} className="settings-feature-pill">
              <span className="settings-feature-pill-icon" aria-hidden="true">
                {renderActionIcon(item.icon, 12)}
              </span>
              {item.label}
            </span>
          ))}
        </div>
      </div>

      <div className="settings-action-list settings-action-list-two">
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
            <span className="settings-action-label-with-icon">
              <span className="settings-action-inline-icon" aria-hidden="true">
                {renderActionIcon(action.id, 14)}
              </span>
              {BUILTIN_LABEL[action.id]}
            </span>
          </label>
        ))}
      </div>

      <div className="settings-custom-list">
        <div className="settings-feature-preview-title">自定义agent</div>
        {customActions.length === 0 ? <p className="settings-hint">还没有自定义agent，点击右上角按钮创建。</p> : null}

        {customActions.map((action) => (
          <div key={action.id} className="settings-custom-item">
            <div className="settings-custom-item-main">
              <span className="settings-custom-icon" aria-hidden="true">
                {renderActionIcon(action.icon, 16)}
              </span>
              <div className="settings-custom-texts">
                <span className="settings-custom-name">{action.name}</span>
                <span className="settings-custom-meta">
                  图标：{resolveCustomIconLabel(action.icon)} ｜ 模型：{action.model.trim() || "跟随优化模型"}
                </span>
              </div>
            </div>

            <div className="settings-custom-item-actions">
              <button
                type="button"
                className="settings-size-btn settings-inline-btn"
                onClick={() => {
                  openEditDialog(action);
                }}
              >
                配置
              </button>

              <label className="settings-custom-item-toggle" aria-label={`启用${action.name}`}>
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
              </label>
            </div>
          </div>
        ))}
      </div>

      {isDialogOpen ? (
        <div
          className="settings-dialog-mask"
          role="dialog"
          aria-modal="true"
          aria-label={editingId ? "编辑自定义功能" : "新增自定义功能"}
          onClick={(event) => {
            if (event.target === event.currentTarget) {
              closeDialog();
            }
          }}
        >
          <div className="settings-dialog-card">
            <div className="settings-dialog-grid-two">
              <label className="settings-field">
                <span className="settings-field-row">
                  <span>功能名称</span>
                  <em className="settings-field-counter">{nameCharCount}/{MAX_CUSTOM_ACTION_NAME_LENGTH}</em>
                </span>
                <input
                  type="text"
                  value={newName}
                  onChange={(event) => {
                    setNewName(clampName(event.target.value));
                  }}
                  placeholder="例如：润色商务语气"
                />
              </label>

              <label className="settings-field">
                <span>模型名称（可选）</span>
                <input
                  type="text"
                  value={newModel}
                  onChange={(event) => {
                    setNewModel(event.target.value);
                  }}
                  placeholder="留空则使用优化模型"
                />
              </label>
            </div>

            <div className="settings-field">
              <span>图标（Lucide 100）</span>
              <div className="settings-icon-grid" role="listbox" aria-label="图标预置">
                {CUSTOM_ACTION_ICON_PRESETS.map((item) => {
                  const active = item.id === newIcon;
                  return (
                    <button
                      key={item.id}
                      type="button"
                      role="option"
                      aria-selected={active}
                      className={`settings-icon-btn ${active ? "active" : ""}`}
                      onClick={() => {
                        setNewIcon(item.id);
                      }}
                      title={item.label}
                    >
                      <span className="settings-icon-glyph" aria-hidden="true">
                        {renderActionIcon(item.id, 14)}
                      </span>
                      <span className="settings-icon-caption">{item.label}</span>
                    </button>
                  );
                })}
              </div>
            </div>

            <div className="settings-field">
              <div className="settings-prompt-head">
                <span>Prompt 模板</span>
                <div className="settings-placeholder-row" aria-label="占位符快捷操作">
                  {PLACEHOLDER_ITEMS.map((item) => (
                    <button
                      key={item.token}
                      type="button"
                      className="settings-placeholder-token"
                      title={item.hint}
                      onClick={() => {
                        void copyPlaceholderToken(item.token);
                      }}
                    >
                      {item.token}
                    </button>
                  ))}
                  {copiedToken ? <span className="settings-placeholder-copied">已复制</span> : null}
                </div>
              </div>
              <div className="settings-prompt-editor">
                <textarea
                  value={newPrompt}
                  placeholder="请输入自定义提示词"
                  onChange={(event) => {
                    setNewPrompt(event.target.value);
                  }}
                />
              </div>
            </div>

            <div className="settings-dialog-actions settings-dialog-actions-right">
              {editingId ? (
                <button
                  type="button"
                  className="settings-size-btn settings-inline-btn"
                  onClick={() => {
                    deleteEditingAction();
                  }}
                >
                  删除
                </button>
              ) : null}

              <button
                type="button"
                className="settings-size-btn settings-inline-btn"
                onClick={() => {
                  closeDialog();
                }}
              >
                取消
              </button>
              <button
                type="button"
                className="settings-api-test-btn"
                disabled={saveDisabled}
                onClick={() => {
                  saveDialog();
                }}
              >
                保存
              </button>
            </div>
          </div>
        </div>
      ) : null}
    </section>
  );
}
