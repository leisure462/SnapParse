import { useEffect, useMemo } from "react";
import {
  DEFAULT_OCR_PROMPT,
  DEFAULT_OPENAI_OCR_BASE_URL,
  DEFAULT_OPENAI_OCR_MODEL,
  GLM_OCR_LOCKED_BASE_URL,
  GLM_OCR_LOCKED_MODEL,
  type AppSettings,
  type OcrProvider
} from "../../../shared/settings";
import { resolveActionBarActions } from "../../action-bar/actions";
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

export default function OcrSettingsSection(props: SettingsSectionProps): JSX.Element {
  const { settings, onChange } = props;
  const isGlmProvider = settings.ocr.provider === "glm-ocr";

  const actionOptions = useMemo(() => {
    return resolveActionBarActions(settings).filter((item) => item.commandWindow);
  }, [settings]);

  useEffect(() => {
    if (!isGlmProvider) {
      return;
    }

    if (
      settings.ocr.baseUrl === GLM_OCR_LOCKED_BASE_URL &&
      settings.ocr.model === GLM_OCR_LOCKED_MODEL
    ) {
      return;
    }

    onChange(
      patchOcr(settings, (ocr) => ({
        ...ocr,
        baseUrl: GLM_OCR_LOCKED_BASE_URL,
        model: GLM_OCR_LOCKED_MODEL
      }))
    );
  }, [isGlmProvider, onChange, settings]);

  useEffect(() => {
    if (actionOptions.length === 0) {
      return;
    }

    if (actionOptions.some((item) => item.id === settings.ocr.postActionId)) {
      return;
    }

    onChange(
      patchOcr(settings, (ocr) => ({
        ...ocr,
        postActionId: actionOptions[0].id
      }))
    );
  }, [actionOptions, onChange, settings]);

  const selectedActionId = actionOptions.some((item) => item.id === settings.ocr.postActionId)
    ? settings.ocr.postActionId
    : (actionOptions[0]?.id ?? "translate");

  return (
    <section className="settings-section" aria-label="OCR配置面板">
      <h2>OCR配置</h2>
      <p className="settings-hint">快捷键进入划屏框选，自动提取图片文字并按条形栏功能继续处理。</p>

      <label className="settings-switch settings-switch-full">
        <input
          type="checkbox"
          checked={settings.ocr.enabled}
          onChange={(event) => {
            onChange(
              patchOcr(settings, (ocr) => ({
                ...ocr,
                enabled: event.target.checked
              }))
            );
          }}
        />
        <span>启用截屏快捷键</span>
      </label>

      <div className="settings-grid-2">
        <label className="settings-field">
          <span>OCR 服务类型</span>
          <select
            value={settings.ocr.provider}
            onChange={(event) => {
              const provider = event.target.value as OcrProvider;
              onChange(
                patchOcr(settings, (ocr) => {
                  let baseUrl = ocr.baseUrl;
                  let model = ocr.model;

                  if (provider === "glm-ocr") {
                    baseUrl = GLM_OCR_LOCKED_BASE_URL;
                    model = GLM_OCR_LOCKED_MODEL;
                  } else if (
                    ocr.provider === "glm-ocr" &&
                    ocr.baseUrl.trim() === GLM_OCR_LOCKED_BASE_URL &&
                    ocr.model.trim() === GLM_OCR_LOCKED_MODEL
                  ) {
                    baseUrl = DEFAULT_OPENAI_OCR_BASE_URL;
                    model = DEFAULT_OPENAI_OCR_MODEL;
                  }

                  return {
                    ...ocr,
                    provider,
                    baseUrl,
                    model
                  };
                })
              );
            }}
          >
            <option value="openai-vision">OpenAI-Compatible Vision</option>
            <option value="glm-ocr">GLM OCR</option>
          </select>
        </label>

        <label className="settings-field">
          <span>自动执行功能</span>
          <select
            value={selectedActionId}
            disabled={actionOptions.length === 0}
            onChange={(event) => {
              onChange(
                patchOcr(settings, (ocr) => ({
                  ...ocr,
                  postActionId: event.target.value
                }))
              );
            }}
          >
            {actionOptions.map((item) => (
              <option key={item.id} value={item.id}>
                {item.label}
              </option>
            ))}
          </select>
        </label>

        <label className="settings-field">
          <span>Base URL</span>
          <input
            type="text"
            value={settings.ocr.baseUrl}
            disabled={isGlmProvider}
            onChange={(event) => {
              onChange(
                patchOcr(settings, (ocr) => ({
                  ...ocr,
                  baseUrl: event.target.value
                }))
              );
            }}
          />
        </label>

        <label className="settings-field">
          <span>API Key</span>
          <input
            type="password"
            value={settings.ocr.apiKey}
            onChange={(event) => {
              onChange(
                patchOcr(settings, (ocr) => ({
                  ...ocr,
                  apiKey: event.target.value
                }))
              );
            }}
          />
        </label>

        <label className="settings-field">
          <span>视觉模型</span>
          <input
            type="text"
            value={settings.ocr.model}
            disabled={isGlmProvider}
            onChange={(event) => {
              onChange(
                patchOcr(settings, (ocr) => ({
                  ...ocr,
                  model: event.target.value
                }))
              );
            }}
          />
        </label>

        <label className="settings-field">
          <span>请求超时（毫秒）</span>
          <input
            type="number"
            min={1000}
            step={500}
            value={settings.ocr.timeoutMs}
            onChange={(event) => {
              const parsed = Number.parseInt(event.target.value, 10);
              onChange(
                patchOcr(settings, (ocr) => ({
                  ...ocr,
                  timeoutMs: Number.isFinite(parsed) ? parsed : ocr.timeoutMs
                }))
              );
            }}
          />
        </label>
      </div>

      {isGlmProvider ? (
        <p className="settings-hint">GLM OCR 使用固定接口与模型：layout_parsing + glm-ocr。</p>
      ) : null}

      <label className="settings-field">
        <span>OCR 提示词</span>
        <textarea
          value={settings.ocr.prompt}
          onChange={(event) => {
            onChange(
              patchOcr(settings, (ocr) => ({
                ...ocr,
                prompt: event.target.value
              }))
            );
          }}
        />
      </label>

      <div className="settings-inline-actions">
        <span className="settings-hint">可留空提示词（适配只需图片输入的 OCR 模型）。</span>
        <button
          type="button"
          className="settings-api-test-btn"
          onClick={() => {
            onChange(
              patchOcr(settings, (ocr) => ({
                ...ocr,
                prompt: DEFAULT_OCR_PROMPT
              }))
            );
          }}
        >
          恢复默认提示词
        </button>
      </div>
    </section>
  );
}
