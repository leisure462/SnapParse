export type ClipboardKind = "text" | "link" | "image";

export interface ClipboardEntry {
  id: string;
  kind: ClipboardKind;
  content: string;
  imageDataUrl: string | null;
  thumbnailDataUrl: string | null;
  copiedAt: string;
  pinned: boolean;
}

export type FilterKind = "all" | ClipboardKind | "favorite";
export type DefaultOpenCategory = FilterKind | "last-used";

export type ThemePreset = "dark";
export type AppLanguage = "zh-CN" | "en-US";
export type PasteBehavior = "copy-only" | "copy-and-hide";
export type SelectionTriggerMode = "auto-detect" | "copy-trigger";
export type SelectionActionKind = "summarize" | "polish" | "explain" | "translate" | "custom";
export type OcrActionKind = "translate" | "summarize" | "polish" | "explain" | "custom";
export type TtsRuntimeMode = "dual-fallback" | "edge-cli-only" | "python-module-only";
export type BuiltinSelectionBarActionKey =
  | "copy"
  | "summarize"
  | "polish"
  | "explain"
  | "translate"
  | "search";
export type SelectionBarActionKey = BuiltinSelectionBarActionKey | `custom:${string}`;

export interface WindowSettings {
  autoHideOnBlur: boolean;
  rememberPosition: boolean;
  rememberMainWindowSize: boolean;
  launchOnSystemStartup: boolean;
  silentStartup: boolean;
  checkUpdatesOnStartup: boolean;
}

export interface SelectionAssistantSettings {
  enabled: boolean;
  mode: SelectionTriggerMode;
  compactMode: boolean;
  barOpacity: number;
  searchUrlTemplate: string;
  minChars: number;
  maxChars: number;
  blockedApps: string[];
  defaultTranslateTo: "zh-CN" | "en-US" | "ja-JP" | "ko-KR";
  resultWindowAlwaysOnTop: boolean;
  rememberResultWindowPosition: boolean;
}

export interface LlmSettings {
  enabled: boolean;
  baseUrl: string;
  apiKey: string;
  model: string;
  temperature: number;
  maxTokens: number;
  timeoutMs: number;
}

export interface CustomAgent {
  id: string;
  name: string;
  icon: string;
  prompt: string;
  enabled: boolean;
  order: number;
}

export interface SelectionBarItemConfig {
  key: SelectionBarActionKey;
  enabled: boolean;
  order: number;
}

export interface AgentSettings {
  custom: CustomAgent[];
  barOrder: SelectionBarItemConfig[];
}

export interface ShortcutSettings {
  toggleMain: string;
  toggleOcr: string;
}

export interface VisionSettings {
  enabled: boolean;
  baseUrl: string;
  apiKey: string;
  model: string;
  temperature: number;
  maxTokens: number;
  timeoutMs: number;
}

export interface OcrSettings {
  enabled: boolean;
  autoRunAfterCapture: boolean;
  defaultAction: OcrActionKind;
  customAgentId: string;
  resultWindowAlwaysOnTop: boolean;
  rememberResultWindowPosition: boolean;
  vision: VisionSettings;
}

export interface TtsSettings {
  runtimeMode: TtsRuntimeMode;
  voiceZhCn: string;
  voiceEnUs: string;
  ratePercent: number;
}

export interface AppearanceSettings {
  blurPx: number;
  saturatePercent: number;
  windowOpacity: number;
  surfaceOpacity: number;
  cardOpacity: number;
  borderOpacity: number;
  shadowOpacity: number;
  cornerRadius: number;
  fontScale: number;
  accentColor: string;
  textColor: string;
  textMutedColor: string;
}

export interface HistorySettings {
  pollMs: number;
  maxItems: number;
  dedupe: boolean;
  captureText: boolean;
  captureLink: boolean;
  captureImage: boolean;
  enableItemGradients: boolean;
  defaultOpenCategory: DefaultOpenCategory;
  defaultCategory: FilterKind;
  pasteBehavior: PasteBehavior;
  collapseTopBar: boolean;
  promoteAfterPaste: boolean;
  openAtTopOnShow: boolean;
  storagePath: string;
}

export interface AppSettings {
  version: number;
  themePreset: ThemePreset;
  language: AppLanguage;
  window: WindowSettings;
  selectionAssistant: SelectionAssistantSettings;
  llm: LlmSettings;
  tts: TtsSettings;
  agents: AgentSettings;
  shortcuts: ShortcutSettings;
  ocr: OcrSettings;
  appearance: AppearanceSettings;
  history: HistorySettings;
}

export interface WindowSettingsPatch {
  autoHideOnBlur?: boolean;
  rememberPosition?: boolean;
  rememberMainWindowSize?: boolean;
  launchOnSystemStartup?: boolean;
  silentStartup?: boolean;
  checkUpdatesOnStartup?: boolean;
}

export interface SelectionAssistantSettingsPatch {
  enabled?: boolean;
  mode?: SelectionTriggerMode;
  compactMode?: boolean;
  barOpacity?: number;
  searchUrlTemplate?: string;
  minChars?: number;
  maxChars?: number;
  blockedApps?: string[];
  defaultTranslateTo?: "zh-CN" | "en-US" | "ja-JP" | "ko-KR";
  resultWindowAlwaysOnTop?: boolean;
  rememberResultWindowPosition?: boolean;
}

export interface LlmSettingsPatch {
  enabled?: boolean;
  baseUrl?: string;
  apiKey?: string;
  model?: string;
  temperature?: number;
  maxTokens?: number;
  timeoutMs?: number;
}

export interface AgentSettingsPatch {
  custom?: CustomAgent[];
  barOrder?: SelectionBarItemConfig[];
}

export interface ShortcutSettingsPatch {
  toggleMain?: string;
  toggleOcr?: string;
}

export interface VisionSettingsPatch {
  enabled?: boolean;
  baseUrl?: string;
  apiKey?: string;
  model?: string;
  temperature?: number;
  maxTokens?: number;
  timeoutMs?: number;
}

export interface OcrSettingsPatch {
  enabled?: boolean;
  autoRunAfterCapture?: boolean;
  defaultAction?: OcrActionKind;
  customAgentId?: string;
  resultWindowAlwaysOnTop?: boolean;
  rememberResultWindowPosition?: boolean;
  vision?: VisionSettingsPatch;
}

export interface TtsSettingsPatch {
  runtimeMode?: TtsRuntimeMode;
  voiceZhCn?: string;
  voiceEnUs?: string;
  ratePercent?: number;
}

export interface AppearanceSettingsPatch {
  blurPx?: number;
  saturatePercent?: number;
  windowOpacity?: number;
  surfaceOpacity?: number;
  cardOpacity?: number;
  borderOpacity?: number;
  shadowOpacity?: number;
  cornerRadius?: number;
  fontScale?: number;
  accentColor?: string;
  textColor?: string;
  textMutedColor?: string;
}

export interface HistorySettingsPatch {
  pollMs?: number;
  maxItems?: number;
  dedupe?: boolean;
  captureText?: boolean;
  captureLink?: boolean;
  captureImage?: boolean;
  enableItemGradients?: boolean;
  defaultOpenCategory?: DefaultOpenCategory;
  defaultCategory?: FilterKind;
  pasteBehavior?: PasteBehavior;
  collapseTopBar?: boolean;
  promoteAfterPaste?: boolean;
  openAtTopOnShow?: boolean;
  storagePath?: string;
}

export interface AppSettingsPatch {
  themePreset?: ThemePreset;
  language?: AppLanguage;
  window?: WindowSettingsPatch;
  selectionAssistant?: SelectionAssistantSettingsPatch;
  llm?: LlmSettingsPatch;
  tts?: TtsSettingsPatch;
  agents?: AgentSettingsPatch;
  shortcuts?: ShortcutSettingsPatch;
  ocr?: OcrSettingsPatch;
  appearance?: AppearanceSettingsPatch;
  history?: HistorySettingsPatch;
}
