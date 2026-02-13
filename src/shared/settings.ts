export type BuiltinActionId = "translate" | "explain" | "summarize" | "optimize" | "search" | "copy";
export type ActionId = BuiltinActionId;
export type TriggerMode = "selection" | "ctrl" | "hotkey";
export type ThemeMode = "light" | "dark" | "system";
export type AppLanguage = "zh-CN" | "en-US";
export type AppFilterMode = "off" | "whitelist" | "blacklist";
export type LogLevel = "error" | "warn" | "info" | "debug";
export type WindowSizePreset = "large" | "medium" | "small";

export const MAX_CUSTOM_ACTION_COUNT = 3;
export const MAX_CUSTOM_ACTION_NAME_LENGTH = 8;

export interface GeneralSettings {
  launchAtStartup: boolean;
  silentStartup: boolean;
  language: AppLanguage;
}

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
    optimize: string;
  };
}

export interface ToolbarAction {
  id: ActionId;
  label: string;
  enabled: boolean;
  order: number;
}

export interface CustomFeatureAction {
  id: string;
  name: string;
  icon: string;
  prompt: string;
  model: string;
  enabled: boolean;
  order: number;
}

export interface ToolbarSettings {
  triggerMode: TriggerMode;
  triggerHotkey: string;
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
  windowSize: WindowSizePreset;
  fontSize: number;
}

export interface FeaturesSettings {
  customActionsEnabled: boolean;
  enabledActions: ActionId[];
  customActions: CustomFeatureAction[];
}

export interface AdvancedSettings {
  appFilterMode: AppFilterMode;
  appList: string[];
  logLevel: LogLevel;
}

export interface AppSettings {
  general: GeneralSettings;
  api: ApiSettings;
  toolbar: ToolbarSettings;
  window: WindowSettings;
  features: FeaturesSettings;
  advanced: AdvancedSettings;
}

export const SETTINGS_SECTION_ORDER = [
  "api",
  "general",
  "toolbar",
  "hotkeys",
  "window",
  "features",
  "advanced"
] as const;

/** Resolve a window size preset to pixel dimensions. */
export function resolveWindowSize(preset: WindowSizePreset): { width: number; height: number } {
  switch (preset) {
    case "large":
      return { width: 680, height: 520 };
    case "medium":
      return { width: 520, height: 400 };
    case "small":
      return { width: 400, height: 320 };
    default:
      return { width: 680, height: 520 };
  }
}

type DeepPartial<T> = {
  [K in keyof T]?: T[K] extends Array<infer U>
    ? U[]
    : T[K] extends object
      ? DeepPartial<T[K]>
      : T[K];
};

function clampChars(value: string, max: number): string {
  return Array.from(value).slice(0, max).join("");
}

function defaultToolbarActions(): ToolbarAction[] {
  return [
    { id: "translate", label: "翻译", enabled: true, order: 0 },
    { id: "explain", label: "解释", enabled: true, order: 1 },
    { id: "summarize", label: "总结", enabled: true, order: 2 },
    { id: "optimize", label: "优化", enabled: true, order: 3 },
    { id: "search", label: "搜索", enabled: true, order: 4 },
    { id: "copy", label: "复制", enabled: true, order: 5 }
  ];
}

export function defaultSettings(): AppSettings {
  const model = "gpt-4o-mini";

  return {
    general: {
      launchAtStartup: false,
      silentStartup: false,
      language: "zh-CN"
    },
    api: {
      baseUrl: "https://api.openai.com/v1",
      apiKey: "",
      model,
      timeoutMs: 30000,
      temperature: 0.3,
      featureModels: {
        translate: model,
        summarize: model,
        explain: model,
        optimize: model
      }
    },
    toolbar: {
      triggerMode: "selection",
      triggerHotkey: "Ctrl+Shift+Space",
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
      windowSize: "large",
      fontSize: 14
    },
    features: {
      customActionsEnabled: false,
      enabledActions: ["translate", "explain", "summarize", "optimize", "search", "copy"],
      customActions: []
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

function mergeGeneral(
  defaults: GeneralSettings,
  incoming: DeepPartial<GeneralSettings> | undefined
): GeneralSettings {
  return incoming ? { ...defaults, ...incoming } : defaults;
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
      ? defaultToolbarActions().map((fallback, index) => {
          const incomingAction = incoming.actions?.find((item) => item.id === fallback.id);
          return {
            ...fallback,
            ...incomingAction,
            order: incomingAction?.order ?? fallback.order ?? index
          };
        })
      : defaults.actions
  };
}

function mergeWindow(
  defaults: WindowSettings,
  incoming: DeepPartial<WindowSettings> | undefined
): WindowSettings {
  if (!incoming) {
    return defaults;
  }

  const merged = { ...defaults, ...incoming };

  // Migrate old windowWidth/windowHeight to windowSize preset
  const raw = incoming as Record<string, unknown>;
  if (!incoming.windowSize && (raw.windowWidth || raw.windowHeight)) {
    const w = Number(raw.windowWidth) || 520;
    if (w >= 600) {
      merged.windowSize = "large";
    } else if (w >= 460) {
      merged.windowSize = "medium";
    } else {
      merged.windowSize = "small";
    }
  }

  // Validate windowSize
  if (!["large", "medium", "small"].includes(merged.windowSize)) {
    merged.windowSize = "large";
  }

  return merged;
}

function mergeFeatures(
  defaults: FeaturesSettings,
  incoming: DeepPartial<FeaturesSettings> | undefined
): FeaturesSettings {
  return incoming
    ? {
        ...defaults,
        ...incoming,
        enabledActions: incoming.enabledActions ?? defaults.enabledActions,
        customActions: incoming.customActions
          ? incoming.customActions.slice(0, MAX_CUSTOM_ACTION_COUNT).map((action, index) => ({
              id: action.id ?? `custom-${index}`,
              name: clampChars(action.name ?? "自定义功能", MAX_CUSTOM_ACTION_NAME_LENGTH),
              icon: action.icon ?? "bot",
              prompt: action.prompt ?? "{{text}}",
              model: action.model ?? "",
              enabled: action.enabled ?? true,
              order: action.order ?? index
            }))
          : defaults.customActions
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
    general: mergeGeneral(defaults.general, partial.general),
    api: mergeApi(defaults.api, partial.api),
    toolbar: mergeToolbar(defaults.toolbar, partial.toolbar),
    window: mergeWindow(defaults.window, partial.window),
    features: mergeFeatures(defaults.features, partial.features),
    advanced: mergeAdvanced(defaults.advanced, partial.advanced)
  };
}

function assertActionIds(actions: ToolbarAction[]): void {
  const validIds = new Set<ActionId>(["translate", "explain", "summarize", "optimize", "search", "copy"]);
  for (const action of actions) {
    if (!validIds.has(action.id)) {
      throw new Error(`Unknown toolbar action: ${action.id}`);
    }
  }
}

function assertCustomActions(actions: CustomFeatureAction[]): void {
  if (actions.length > MAX_CUSTOM_ACTION_COUNT) {
    throw new Error(`features.customActions supports up to ${MAX_CUSTOM_ACTION_COUNT} items`);
  }

  const ids = new Set<string>();
  for (const action of actions) {
    if (!action.id.trim()) {
      throw new Error("features.customActions[].id must not be empty");
    }
    if (ids.has(action.id)) {
      throw new Error(`features.customActions has duplicate id: ${action.id}`);
    }
    ids.add(action.id);

    if (!action.name.trim()) {
      throw new Error("features.customActions[].name must not be empty");
    }

    if (Array.from(action.name.trim()).length > MAX_CUSTOM_ACTION_NAME_LENGTH) {
      throw new Error(`features.customActions[].name must be <= ${MAX_CUSTOM_ACTION_NAME_LENGTH} chars`);
    }

    if (!action.prompt.trim()) {
      throw new Error("features.customActions[].prompt must not be empty");
    }
  }
}

function assertEnabledActionIds(actionIds: ActionId[]): void {
  const validIds = new Set<ActionId>(["translate", "explain", "summarize", "optimize", "search", "copy"]);
  for (const id of actionIds) {
    if (!validIds.has(id)) {
      throw new Error(`Unknown enabled feature action: ${id}`);
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
    const fallbackModel =
      merged.api.featureModels.translate.trim() ||
      merged.api.featureModels.summarize.trim() ||
      merged.api.featureModels.explain.trim() ||
      merged.api.featureModels.optimize.trim();

    if (!fallbackModel) {
      throw new Error("api.model and feature models must not all be empty");
    }

    merged.api.model = fallbackModel;
  }

  if (!merged.toolbar.triggerHotkey.trim()) {
    throw new Error("toolbar.triggerHotkey must not be empty");
  }

  if (!["zh-CN", "en-US"].includes(merged.general.language)) {
    throw new Error("general.language must be zh-CN or en-US");
  }

  assertNumberRange("api.timeoutMs", merged.api.timeoutMs, 1000, 120000);
  assertNumberRange("api.temperature", merged.api.temperature, 0, 2);
  assertNumberRange("window.fontSize", merged.window.fontSize, 10, 24);

  if (!["large", "medium", "small"].includes(merged.window.windowSize)) {
    throw new Error("window.windowSize must be large, medium, or small");
  }

  if (merged.toolbar.actions.length === 0) {
    throw new Error("toolbar.actions must contain at least one action");
  }

  assertActionIds(merged.toolbar.actions);
  assertEnabledActionIds(merged.features.enabledActions);
  assertCustomActions(merged.features.customActions);

  return merged;
}
