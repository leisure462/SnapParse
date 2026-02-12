export type ActionId = "translate" | "explain" | "summarize" | "search" | "copy";
export type TriggerMode = "selection" | "ctrl" | "hotkey";
export type ThemeMode = "light" | "dark" | "system";
export type AppFilterMode = "off" | "whitelist" | "blacklist";
export type LogLevel = "error" | "warn" | "info" | "debug";

export interface ApiSettings {
  baseUrl: string;
  apiKey: string;
  model: string;
  timeoutMs: number;
  temperature: number;
  featureModels: {
    translate: string;
    summarize: string;
    explain: string;
  };
}

export interface ToolbarAction {
  id: ActionId;
  label: string;
  enabled: boolean;
  order: number;
}

export interface ToolbarSettings {
  triggerMode: TriggerMode;
  compactMode: boolean;
  showLabel: boolean;
  themeMode: ThemeMode;
  actions: ToolbarAction[];
}

export interface WindowSettings {
  followToolbar: boolean;
  rememberSize: boolean;
  autoClose: boolean;
  autoPin: boolean;
  opacity: number;
  windowWidth: number;
  windowHeight: number;
  fontSize: number;
}

export interface FeaturesSettings {
  customActionsEnabled: boolean;
  enabledActions: ActionId[];
}

export interface AdvancedSettings {
  appFilterMode: AppFilterMode;
  appList: string[];
  logLevel: LogLevel;
}

export interface AppSettings {
  api: ApiSettings;
  toolbar: ToolbarSettings;
  window: WindowSettings;
  features: FeaturesSettings;
  advanced: AdvancedSettings;
}

export const SETTINGS_SECTION_ORDER = [
  "api",
  "toolbar",
  "window",
  "features",
  "advanced"
] as const;

type DeepPartial<T> = {
  [K in keyof T]?: T[K] extends Array<infer U>
    ? U[]
    : T[K] extends object
      ? DeepPartial<T[K]>
      : T[K];
};

function defaultToolbarActions(): ToolbarAction[] {
  return [
    { id: "translate", label: "翻译", enabled: true, order: 0 },
    { id: "explain", label: "解释", enabled: true, order: 1 },
    { id: "summarize", label: "总结", enabled: true, order: 2 },
    { id: "search", label: "搜索", enabled: true, order: 3 },
    { id: "copy", label: "复制", enabled: true, order: 4 }
  ];
}

export function defaultSettings(): AppSettings {
  const model = "gpt-4o-mini";

  return {
    api: {
      baseUrl: "https://api.openai.com/v1",
      apiKey: "",
      model,
      timeoutMs: 30000,
      temperature: 0.3,
      featureModels: {
        translate: model,
        summarize: model,
        explain: model
      }
    },
    toolbar: {
      triggerMode: "selection",
      compactMode: false,
      showLabel: true,
      themeMode: "dark",
      actions: defaultToolbarActions()
    },
    window: {
      followToolbar: true,
      rememberSize: true,
      autoClose: false,
      autoPin: false,
      opacity: 1,
      windowWidth: 520,
      windowHeight: 420,
      fontSize: 14
    },
    features: {
      customActionsEnabled: false,
      enabledActions: ["translate", "explain", "summarize", "search", "copy"]
    },
    advanced: {
      appFilterMode: "off",
      appList: [],
      logLevel: "info"
    }
  };
}

function mergeApi(
  defaults: ApiSettings,
  incoming: DeepPartial<ApiSettings> | undefined
): ApiSettings {
  if (!incoming) {
    return defaults;
  }

  return {
    ...defaults,
    ...incoming,
    featureModels: {
      ...defaults.featureModels,
      ...incoming.featureModels
    }
  };
}

function mergeToolbar(
  defaults: ToolbarSettings,
  incoming: DeepPartial<ToolbarSettings> | undefined
): ToolbarSettings {
  if (!incoming) {
    return defaults;
  }

  return {
    ...defaults,
    ...incoming,
    actions: incoming.actions
      ? incoming.actions.map((action, index) => ({
          ...defaultToolbarActions()[index],
          ...action,
          order: action.order ?? index
        }))
      : defaults.actions
  };
}

function mergeWindow(
  defaults: WindowSettings,
  incoming: DeepPartial<WindowSettings> | undefined
): WindowSettings {
  return incoming ? { ...defaults, ...incoming } : defaults;
}

function mergeFeatures(
  defaults: FeaturesSettings,
  incoming: DeepPartial<FeaturesSettings> | undefined
): FeaturesSettings {
  return incoming
    ? {
        ...defaults,
        ...incoming,
        enabledActions: incoming.enabledActions ?? defaults.enabledActions
      }
    : defaults;
}

function mergeAdvanced(
  defaults: AdvancedSettings,
  incoming: DeepPartial<AdvancedSettings> | undefined
): AdvancedSettings {
  return incoming
    ? {
        ...defaults,
        ...incoming,
        appList: incoming.appList ?? defaults.appList
      }
    : defaults;
}

export function mergeSettings(partial: DeepPartial<AppSettings> = {}): AppSettings {
  const defaults = defaultSettings();

  return {
    api: mergeApi(defaults.api, partial.api),
    toolbar: mergeToolbar(defaults.toolbar, partial.toolbar),
    window: mergeWindow(defaults.window, partial.window),
    features: mergeFeatures(defaults.features, partial.features),
    advanced: mergeAdvanced(defaults.advanced, partial.advanced)
  };
}

function assertActionIds(actions: ToolbarAction[]): void {
  const validIds = new Set<ActionId>(["translate", "explain", "summarize", "search", "copy"]);
  for (const action of actions) {
    if (!validIds.has(action.id)) {
      throw new Error(`Unknown toolbar action: ${action.id}`);
    }
  }
}

function assertUrl(urlString: string): void {
  try {
    const parsed = new URL(urlString);
    if (!(parsed.protocol === "http:" || parsed.protocol === "https:")) {
      throw new Error("URL protocol must be http or https");
    }
  } catch (error) {
    throw new Error(`Invalid API baseUrl: ${urlString}`, { cause: error });
  }
}

function assertNumberRange(label: string, value: number, min: number, max: number): void {
  if (Number.isNaN(value) || value < min || value > max) {
    throw new Error(`${label} must be in range [${min}, ${max}]`);
  }
}

export function validateSettings(input: DeepPartial<AppSettings> = {}): AppSettings {
  const merged = mergeSettings(input);

  assertUrl(merged.api.baseUrl);

  if (!merged.api.model.trim()) {
    throw new Error("api.model must not be empty");
  }

  assertNumberRange("api.timeoutMs", merged.api.timeoutMs, 1000, 120000);
  assertNumberRange("api.temperature", merged.api.temperature, 0, 2);
  assertNumberRange("window.opacity", merged.window.opacity, 0.2, 1);
  assertNumberRange("window.windowWidth", merged.window.windowWidth, 320, 1600);
  assertNumberRange("window.windowHeight", merged.window.windowHeight, 280, 1200);
  assertNumberRange("window.fontSize", merged.window.fontSize, 10, 24);

  if (merged.toolbar.actions.length === 0) {
    throw new Error("toolbar.actions must contain at least one action");
  }

  assertActionIds(merged.toolbar.actions);

  return merged;
}
