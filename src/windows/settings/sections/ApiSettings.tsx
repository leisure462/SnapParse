import type { AppSettings } from "../../../shared/settings";
import type { SettingsSectionProps } from "./sectionTypes";

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
