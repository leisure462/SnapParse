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
  return {
    ...settings,
    api: updater(settings.api)
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
        api: settings.api
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
      <p className="settings-hint">OpenAI-Compatible: base_url + api_key + model</p>

      <label className="settings-field">
        <span>Base URL</span>
        <input
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

      <p className="settings-warning">当前按你的要求：API Key 明文存储在 settings.json，请注意本机安全。</p>

      <label className="settings-field">
        <span>默认模型</span>
        <input
          value={settings.api.model}
          onChange={(event) => {
            onChange(
              patchApi(settings, (api) => ({
                ...api,
                model: event.target.value
              }))
            );
          }}
        />
      </label>

      <div className="settings-inline-actions">
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
        <span className={`settings-api-test-status ${testStatus}`}>{testMessage || "可先填写参数再测试"}</span>
      </div>

      <div className="settings-grid-3">
        <label className="settings-field">
          <span>翻译模型</span>
          <input
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
      </div>
    </section>
  );
}
