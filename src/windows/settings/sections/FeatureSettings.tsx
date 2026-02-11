import type { AppSettings, ActionId } from "../../../shared/settings";
import type { SettingsSectionProps } from "./sectionTypes";

const ACTION_LABEL: Record<ActionId, string> = {
  translate: "翻译",
  explain: "解释",
  summarize: "总结",
  search: "搜索",
  copy: "复制"
};

function patchFeatures(
  settings: AppSettings,
  updater: (features: AppSettings["features"]) => AppSettings["features"]
): AppSettings {
  return {
    ...settings,
    features: updater(settings.features)
  };
}

function patchToolbarActions(
  settings: AppSettings,
  actionId: ActionId,
  enabled: boolean
): AppSettings {
  return {
    ...settings,
    toolbar: {
      ...settings.toolbar,
      actions: settings.toolbar.actions.map((item) =>
        item.id === actionId
          ? {
              ...item,
              enabled
            }
          : item
      )
    }
  };
}

export default function FeatureSettingsSection(props: SettingsSectionProps): JSX.Element {
  const { settings, onChange } = props;

  return (
    <section className="settings-section" aria-label="功能配置面板">
      <h2>功能</h2>
      <p className="settings-hint">控制工具栏可见功能，后续可扩展自定义 Prompt 功能。</p>

      <label className="settings-switch">
        <input
          type="checkbox"
          checked={settings.features.customActionsEnabled}
          onChange={(event) => {
            onChange(
              patchFeatures(settings, (features) => ({
                ...features,
                customActionsEnabled: event.target.checked
              }))
            );
          }}
        />
        <span>启用自定义功能（预留）</span>
      </label>

      <div className="settings-action-list">
        {settings.toolbar.actions.map((action) => (
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
                  patchFeatures(patchToolbarActions(settings, action.id, enabled), (features) => ({
                    ...features,
                    enabledActions
                  }))
                );
              }}
            />
            <span>{ACTION_LABEL[action.id]}</span>
          </label>
        ))}
      </div>
    </section>
  );
}
