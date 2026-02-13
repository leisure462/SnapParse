import { invoke } from "@tauri-apps/api/core";
import { useState } from "react";
import type { AppSettings } from "../../../shared/settings";
import type { SettingsSectionProps } from "./sectionTypes";

interface ApiConnectionTestResult {
  model: string;
  message: string;
  elapsedMs: number;
}

function patchApi(
  settings: AppSettings,
  updater: (api: AppSettings["api"]) => AppSettings["api"]
): AppSettings {
  const nextApi = updater(settings.api);

  const fallbackModel =
    nextApi.featureModels.translate.trim() ||
    nextApi.featureModels.summarize.trim() ||
    nextApi.featureModels.explain.trim() ||
    nextApi.featureModels.optimize.trim() ||
    "gpt-4o-mini";

  return {
    ...settings,
    api: {
      ...nextApi,
      model: nextApi.model.trim() ? nextApi.model : fallbackModel
    }
  };
}

export default function ApiSettingsSection(props: SettingsSectionProps): JSX.Element {
  const { settings, onChange } = props;
  const [testStatus, setTestStatus] = useState<"idle" | "testing" | "success" | "error">("idle");
  const [testMessage, setTestMessage] = useState("");

  const runApiTest = async (): Promise<void> => {
    setTestStatus("testing");
    setTestMessage("正在测试 API 连接...");

    try {
      const result = await invoke<ApiConnectionTestResult>("test_api_connection", {
        api: {
          ...settings.api,
          model:
            settings.api.featureModels.translate.trim() ||
            settings.api.featureModels.summarize.trim() ||
            settings.api.featureModels.explain.trim() ||
            settings.api.featureModels.optimize.trim() ||
            settings.api.model
        }
      });

      setTestStatus("success");
      setTestMessage(`测试通过：${result.model}（${result.elapsedMs}ms） ${result.message}`);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      setTestStatus("error");
      setTestMessage(`测试失败：${message}`);
    }
  };

  return (
    <section className="settings-section" aria-label="API配置面板">
      <h2>API配置</h2>
      <p className="settings-hint">OpenAI-Compatible: base_url + api_key + feature models</p>

      <label className="settings-field">
        <span>Base URL</span>
        <input
          type="text"
          value={settings.api.baseUrl}
          onChange={(event) => {
            onChange(
              patchApi(settings, (api) => ({
                ...api,
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
          value={settings.api.apiKey}
          onChange={(event) => {
            onChange(
              patchApi(settings, (api) => ({
                ...api,
                apiKey: event.target.value
              }))
            );
          }}
        />
      </label>

      <div className="settings-inline-actions">
        <span className={`settings-api-test-status ${testStatus}`}>{testMessage || "可先填写参数再测试"}</span>
        <button
          type="button"
          className="settings-api-test-btn"
          onClick={() => {
            void runApiTest();
          }}
          disabled={testStatus === "testing"}
        >
          {testStatus === "testing" ? "测试中..." : "测试 API"}
        </button>
      </div>

      <div className="settings-grid-3">
        <label className="settings-field">
          <span>翻译模型</span>
          <input
            type="text"
            value={settings.api.featureModels.translate}
            onChange={(event) => {
              onChange(
                patchApi(settings, (api) => ({
                  ...api,
                  featureModels: {
                    ...api.featureModels,
                    translate: event.target.value
                  }
                }))
              );
            }}
          />
        </label>

        <label className="settings-field">
          <span>总结模型</span>
          <input
            type="text"
            value={settings.api.featureModels.summarize}
            onChange={(event) => {
              onChange(
                patchApi(settings, (api) => ({
                  ...api,
                  featureModels: {
                    ...api.featureModels,
                    summarize: event.target.value
                  }
                }))
              );
            }}
          />
        </label>

        <label className="settings-field">
          <span>解释模型</span>
          <input
            type="text"
            value={settings.api.featureModels.explain}
            onChange={(event) => {
              onChange(
                patchApi(settings, (api) => ({
                  ...api,
                  featureModels: {
                    ...api.featureModels,
                    explain: event.target.value
                  }
                }))
              );
            }}
          />
        </label>

        <label className="settings-field">
          <span>优化模型</span>
          <input
            type="text"
            value={settings.api.featureModels.optimize}
            onChange={(event) => {
              onChange(
                patchApi(settings, (api) => ({
                  ...api,
                  featureModels: {
                    ...api.featureModels,
                    optimize: event.target.value
                  }
                }))
              );
            }}
          />
        </label>
      </div>
    </section>
  );
}
