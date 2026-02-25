
import { invoke } from "@tauri-apps/api/core";
import { getVersion } from "@tauri-apps/api/app";
import { getCurrentWebviewWindow } from "@tauri-apps/api/webviewWindow";
import { relaunch } from "@tauri-apps/plugin-process";
import { check } from "@tauri-apps/plugin-updater";
import {
  Activity,
  Archive,
  Bell,
  BookOpen,
  Bot,
  Brain,
  Briefcase,
  Bug,
  Calculator,
  Calendar,
  Camera,
  CheckCircle2,
  ChevronDown,
  ChevronUp,
  ChevronsUpDown,
  ClipboardList,
  Cloud,
  Code2,
  Compass,
  Copy,
  Cpu,
  Crown,
  Database,
  Download,
  ExternalLink,
  Feather,
  FileDown,
  FileUp,
  FileText,
  FolderOpen,
  Gem,
  Globe2,
  Heart,
  History,
  Image as ImageIcon,
  Info,
  Keyboard,
  KeyRound,
  Landmark,
  Languages,
  Layers,
  Lightbulb,
  Lock,
  Mail,
  Link2,
  MessageSquare,
  Mic,
  Minus,
  Monitor,
  Moon,
  Palette,
  PenTool,
  Pin,
  PinOff,
  RefreshCw,
  Rocket,
  RotateCcw,
  Ruler,
  ScanSearch,
  Search,
  Settings,
  Shield,
  SlidersHorizontal,
  Sparkles,
  Star,
  Sun,
  Terminal,
  Timer,
  Trash2,
  Type,
  Volume2,
  Wand2,
  Zap,
  X
} from "lucide-react";
import type { LucideIcon } from "lucide-react";
import type { KeyboardEvent as ReactKeyboardEvent, WheelEvent as ReactWheelEvent } from "react";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import type {
  AppSettings,
  AppSettingsPatch,
  BuiltinSelectionBarActionKey,
  ClipboardEntry,
  CustomAgent,
  FilterKind,
  OcrActionKind,
  PasteBehavior,
  SelectionActionKind,
  SelectionBarActionKey,
  SelectionBarItemConfig,
  SelectionTriggerMode,
  ThemePreset,
  TtsRuntimeMode
} from "./types";
import appLogo from "../icon_transparent.png";

const SETTINGS_UPDATED_EVENT = "snapparse://settings-updated";
const FALLBACK_SHORTCUT = "Alt+Space";
const FALLBACK_OCR_SHORTCUT = "Alt+Shift+Space";

const POLL_MS_RANGE = { min: 400, max: 5000 };
const HISTORY_MAX_RANGE = { min: 20, max: 500 };
const SELECTION_AUTO_HIDE_RANGE = { min: 800, max: 30000 };
const SELECTION_MIN_CHARS_RANGE = { min: 1, max: 64 };
const SELECTION_MAX_CHARS_RANGE = { min: 128, max: 100000 };
const LLM_TEMPERATURE_RANGE = { min: 0, max: 2 };
const MODEL_MAX_TOKENS_RANGE = { min: 128, max: 8192 };
const MODEL_TIMEOUT_MS_RANGE = { min: 5000, max: 120000 };
const OCR_VISION_MAX_TOKENS_RANGE = { min: 256, max: 8192 };
const TTS_RATE_PERCENT_RANGE = { min: -50, max: 100 };

const FALLBACK_SETTINGS: AppSettings = {
  version: 8,
  themePreset: "deep-black",
  language: "zh-CN",
  window: {
    autoHideOnBlur: true,
    rememberPosition: true,
    rememberMainWindowSize: true,
    launchOnSystemStartup: false,
    silentStartup: false,
    checkUpdatesOnStartup: false
  },
  selectionAssistant: {
    enabled: true,
    mode: "auto-detect",
    showIconAnimation: true,
    compactMode: false,
    autoHideMs: 5000,
    searchUrlTemplate: "https://www.google.com/search?q={query}",
    minChars: 2,
    maxChars: 12000,
    blockedApps: [],
    defaultTranslateTo: "en-US",
    resultWindowAlwaysOnTop: true
  },
  llm: {
    enabled: true,
    baseUrl: "https://api.openai.com/v1/chat/completions",
    apiKey: "",
    model: "gpt-4o-mini",
    temperature: 0.3,
    maxTokens: 1024,
    timeoutMs: 30000
  },
  tts: {
    runtimeMode: "dual-fallback",
    voiceZhCn: "zh-CN-XiaoxiaoNeural",
    voiceEnUs: "en-US-JennyNeural",
    ratePercent: 0
  },
  agents: {
    custom: [],
    barOrder: [
      { key: "copy", enabled: true, order: 0 },
      { key: "summarize", enabled: true, order: 1 },
      { key: "polish", enabled: true, order: 2 },
      { key: "explain", enabled: true, order: 3 },
      { key: "translate", enabled: true, order: 4 },
      { key: "search", enabled: true, order: 5 }
    ]
  },
  shortcuts: {
    toggleMain: FALLBACK_SHORTCUT,
    toggleOcr: FALLBACK_OCR_SHORTCUT
  },
  ocr: {
    enabled: true,
    autoRunAfterCapture: true,
    defaultAction: "translate",
    customAgentId: "",
    resultWindowAlwaysOnTop: true,
    vision: {
      enabled: false,
      baseUrl: "https://api.openai.com/v1/chat/completions",
      apiKey: "",
      model: "gpt-4o-mini",
      temperature: 0,
      maxTokens: 2048,
      timeoutMs: 30000
    }
  },
  history: {
    pollMs: 1200,
    maxItems: 120,
    dedupe: true,
    captureText: true,
    captureLink: true,
    captureImage: true,
    defaultCategory: "all",
    pasteBehavior: "copy-and-hide",
    collapseTopBar: false,
    storagePath: ""
  }
};

interface FilterOption {
  key: FilterKind;
  label: string;
  icon: LucideIcon;
}

interface ThemeOption {
  key: ThemePreset;
  label: string;
}

type TranslateLanguageCode = "auto" | "zh-CN" | "en-US" | "ja-JP" | "ko-KR";
type TranslateTargetLanguageCode = Exclude<TranslateLanguageCode, "auto">;

interface SelectionDetectedPayload {
  text: string;
  x: number;
  y: number;
  mode: SelectionTriggerMode;
}

interface SelectionResultPayload {
  requestId: string;
  action: string;
  sourceText: string;
  outputText: string;
  translateFrom?: string | null;
  translateTo?: string | null;
  customAgentName?: string | null;
  customAgentIcon?: string | null;
  isStreaming: boolean;
  errorMessage?: string | null;
}

interface OcrResultPayload {
  requestId: string;
  action: string;
  ocrText: string;
  outputText: string;
  translateFrom?: string | null;
  translateTo?: string | null;
  customAgentName?: string | null;
  customAgentIcon?: string | null;
  isStreaming: boolean;
  errorMessage?: string | null;
}

interface TtsSynthesizeResult {
  audioBase64: string;
  mimeType: string;
  voiceUsed: string;
}

function decodeBase64ToBytes(base64: string) {
  const binary = window.atob(base64);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i += 1) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes;
}

function createAudioFromTtsResponse(response: TtsSynthesizeResult) {
  const mimeType = response?.mimeType?.trim() || "audio/mpeg";
  const bytes = decodeBase64ToBytes(response.audioBase64 || "");
  const blob = new Blob([bytes], { type: mimeType });
  const objectUrl = URL.createObjectURL(blob);
  return {
    audio: new Audio(objectUrl),
    revoke: () => URL.revokeObjectURL(objectUrl)
  };
}

interface SettingGroup {
  key:
    | "general"
    | "clipboard"
    | "dataBackup"
    | "selectionAssistant"
    | "smartOcr"
    | "tts"
    | "shortcuts"
    | "about";
  label: string;
  icon: LucideIcon;
  description: string;
}

const FILTER_OPTIONS: FilterOption[] = [
  { key: "all", label: "全部", icon: ClipboardList },
  { key: "image", label: "图片", icon: ImageIcon },
  { key: "link", label: "链接", icon: Link2 },
  { key: "text", label: "文本", icon: Type },
  { key: "favorite", label: "收藏", icon: Star }
];

const THEME_OPTIONS: ThemeOption[] = [
  { key: "blue", label: "Blue" },
  { key: "deep-black", label: "Black" },
  { key: "gray", label: "Gray" },
  { key: "white", label: "White" }
];

const PASTE_BEHAVIOR_OPTIONS: Array<{ key: PasteBehavior; label: string }> = [
  { key: "copy-and-hide", label: "复制后自动隐藏" },
  { key: "copy-only", label: "仅复制，不隐藏" }
];

const SELECTION_MODE_OPTIONS: Array<{ key: SelectionTriggerMode; label: string }> = [
  { key: "auto-detect", label: "自动检测（默认）" },
  { key: "copy-trigger", label: "复制触发（稳妥）" }
];

const OCR_ACTION_OPTIONS: Array<{ key: OcrActionKind; label: string }> = [
  { key: "translate", label: "翻译（默认）" },
  { key: "summarize", label: "总结" },
  { key: "polish", label: "优化" },
  { key: "explain", label: "解释" },
  { key: "custom", label: "自定义 Agent" }
];
const OCR_CUSTOM_ACTION_PREFIX = "custom:";

const TTS_RUNTIME_MODE_OPTIONS: Array<{ key: TtsRuntimeMode; label: string }> = [
  { key: "dual-fallback", label: "双通道回退（推荐）" },
  { key: "edge-cli-only", label: "仅 edge-tts 命令" },
  { key: "python-module-only", label: "仅 python -m edge_tts" }
];

const TTS_ZH_VOICE_OPTIONS = [
  { key: "zh-CN-XiaoxiaoNeural", label: "Xiaoxiao (女声)" },
  { key: "zh-CN-YunxiNeural", label: "Yunxi (男声)" },
  { key: "zh-CN-XiaoyiNeural", label: "Xiaoyi (女声)" },
  { key: "zh-CN-YunjianNeural", label: "Yunjian (男声)" }
];

const TTS_EN_VOICE_OPTIONS = [
  { key: "en-US-JennyNeural", label: "Jenny (Female)" },
  { key: "en-US-AriaNeural", label: "Aria (Female)" },
  { key: "en-US-GuyNeural", label: "Guy (Male)" },
  { key: "en-US-DavisNeural", label: "Davis (Male)" }
];

const TRANSLATE_LANGUAGE_OPTIONS: Array<{ key: TranslateLanguageCode; label: string }> = [
  { key: "auto", label: "自动检测" },
  { key: "zh-CN", label: "简体中文" },
  { key: "en-US", label: "English" },
  { key: "ja-JP", label: "日本語" },
  { key: "ko-KR", label: "한국어" }
];

const SETTING_GROUPS: SettingGroup[] = [
  {
    key: "general",
    label: "通用设置",
    icon: SlidersHorizontal,
    description: "外观、窗口行为与通用偏好"
  },
  {
    key: "clipboard",
    label: "剪贴板",
    icon: ClipboardList,
    description: "历史采集、展示分类与粘贴行为"
  },
  {
    key: "selectionAssistant",
    label: "划词助手",
    icon: Bot,
    description: "划词触发、模型 API 与自定义 Agent"
  },
  {
    key: "smartOcr",
    label: "智能 OCR",
    icon: ScanSearch,
    description: "屏幕框选识别、视觉模型与处理策略"
  },
  {
    key: "tts",
    label: "TTS 设置",
    icon: Volume2,
    description: "语音服务、语言音色与语速"
  },
  {
    key: "shortcuts",
    label: "快捷键",
    icon: Keyboard,
    description: "全局呼出/隐藏快捷键"
  },
  {
    key: "dataBackup",
    label: "数据备份",
    icon: Database,
    description: "存储位置、导入导出与数据维护"
  },
  {
    key: "about",
    label: "关于与诊断",
    icon: Info,
    description: "运行状态、版本信息与更新管理"
  }
];

const BUILTIN_SELECTION_BAR_ACTIONS: Array<{
  key: BuiltinSelectionBarActionKey;
  label: string;
  icon: LucideIcon;
  action?: SelectionActionKind;
  direct?: "copy" | "search";
}> = [
  { key: "copy", label: "复制", icon: Copy, direct: "copy" },
  { key: "summarize", label: "总结", icon: ScanSearch, action: "summarize" },
  { key: "polish", label: "优化", icon: SlidersHorizontal, action: "polish" },
  { key: "explain", label: "解释", icon: Info, action: "explain" },
  { key: "translate", label: "翻译", icon: Languages, action: "translate" },
  { key: "search", label: "搜索", icon: Search, direct: "search" }
];

const BUILTIN_SELECTION_BAR_ACTION_MAP = BUILTIN_SELECTION_BAR_ACTIONS.reduce(
  (acc, item) => {
    acc[item.key] = item;
    return acc;
  },
  {} as Record<BuiltinSelectionBarActionKey, (typeof BUILTIN_SELECTION_BAR_ACTIONS)[number]>
);

const CUSTOM_AGENT_ICON_OPTIONS: Array<{ key: string; label: string; icon: LucideIcon }> = [
  { key: "Activity", label: "Activity", icon: Activity },
  { key: "Archive", label: "Archive", icon: Archive },
  { key: "Bell", label: "Bell", icon: Bell },
  { key: "Sparkles", label: "Sparkles", icon: Sparkles },
  { key: "Bot", label: "Bot", icon: Bot },
  { key: "Info", label: "Info", icon: Info },
  { key: "Brain", label: "Brain", icon: Brain },
  { key: "Briefcase", label: "Briefcase", icon: Briefcase },
  { key: "Bug", label: "Bug", icon: Bug },
  { key: "Calculator", label: "Calculator", icon: Calculator },
  { key: "Calendar", label: "Calendar", icon: Calendar },
  { key: "Camera", label: "Camera", icon: Camera },
  { key: "CheckCircle2", label: "CheckCircle2", icon: CheckCircle2 },
  { key: "Cloud", label: "Cloud", icon: Cloud },
  { key: "Lightbulb", label: "Lightbulb", icon: Lightbulb },
  { key: "MessageSquare", label: "Message", icon: MessageSquare },
  { key: "BookOpen", label: "Book", icon: BookOpen },
  { key: "FileText", label: "FileText", icon: FileText },
  { key: "Code2", label: "Code2", icon: Code2 },
  { key: "Compass", label: "Compass", icon: Compass },
  { key: "Cpu", label: "Cpu", icon: Cpu },
  { key: "Crown", label: "Crown", icon: Crown },
  { key: "Feather", label: "Feather", icon: Feather },
  { key: "Gem", label: "Gem", icon: Gem },
  { key: "Heart", label: "Heart", icon: Heart },
  { key: "KeyRound", label: "Key", icon: KeyRound },
  { key: "Landmark", label: "Landmark", icon: Landmark },
  { key: "Layers", label: "Layers", icon: Layers },
  { key: "Lock", label: "Lock", icon: Lock },
  { key: "Mail", label: "Mail", icon: Mail },
  { key: "Mic", label: "Mic", icon: Mic },
  { key: "Monitor", label: "Monitor", icon: Monitor },
  { key: "Palette", label: "Palette", icon: Palette },
  { key: "Terminal", label: "Terminal", icon: Terminal },
  { key: "PenTool", label: "PenTool", icon: PenTool },
  { key: "Ruler", label: "Ruler", icon: Ruler },
  { key: "Sun", label: "Sun", icon: Sun },
  { key: "Timer", label: "Timer", icon: Timer },
  { key: "Wand2", label: "Wand", icon: Wand2 },
  { key: "Rocket", label: "Rocket", icon: Rocket },
  { key: "Zap", label: "Zap", icon: Zap },
  { key: "Globe2", label: "Globe", icon: Globe2 },
  { key: "Search", label: "Search", icon: Search },
  { key: "Languages", label: "Languages", icon: Languages },
  { key: "Copy", label: "Copy", icon: Copy },
  { key: "Star", label: "Star", icon: Star },
  { key: "Link2", label: "Link2", icon: Link2 },
  { key: "Type", label: "Type", icon: Type },
  { key: "ScanSearch", label: "ScanSearch", icon: ScanSearch },
  { key: "SlidersHorizontal", label: "Sliders", icon: SlidersHorizontal },
  { key: "ClipboardList", label: "Clipboard", icon: ClipboardList },
  { key: "ImageIcon", label: "Image", icon: ImageIcon },
  { key: "Shield", label: "Shield", icon: Shield },
  { key: "Moon", label: "Moon", icon: Moon },
  { key: "Pin", label: "Pin", icon: Pin },
  { key: "Settings", label: "Settings", icon: Settings },
  { key: "ExternalLink", label: "External", icon: ExternalLink }
];
const CUSTOM_AGENT_ICON_MAP: Record<string, LucideIcon> = CUSTOM_AGENT_ICON_OPTIONS.reduce(
  (acc, item) => {
    acc[item.key.toLowerCase()] = item.icon;
    return acc;
  },
  {} as Record<string, LucideIcon>
);

const CUSTOM_AGENT_MAX_COUNT = 30;
const CUSTOM_AGENT_NAME_MAX_UNITS = 8;
const CUSTOM_AGENT_PROMPT_TEMPLATE = "Process the following text according to my instruction:\n{text}";
const MAX_SELECTION_BAR_ENABLED_ITEMS = 8;

function isCjkChar(char: string) {
  return /[\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff]/u.test(char);
}

function countNameUnits(name: string) {
  let units = 0;
  for (const char of name) {
    units += isCjkChar(char) ? 2 : 1;
  }
  return units;
}

function trimNameByUnits(name: string, maxUnits = CUSTOM_AGENT_NAME_MAX_UNITS) {
  let units = 0;
  let next = "";
  for (const char of name.trim()) {
    const weight = isCjkChar(char) ? 2 : 1;
    if (units + weight > maxUnits) break;
    next += char;
    units += weight;
  }
  return next.trim();
}

function parseCustomAgentActionKey(key: string): string | null {
  if (!key.startsWith("custom:")) return null;
  const id = key.slice("custom:".length).trim();
  return id || null;
}

function isBuiltinSelectionBarActionKey(value: string): value is BuiltinSelectionBarActionKey {
  return Object.prototype.hasOwnProperty.call(BUILTIN_SELECTION_BAR_ACTION_MAP, value);
}

function defaultSelectionBarOrder(customAgents: CustomAgent[]): SelectionBarItemConfig[] {
  const builtins = BUILTIN_SELECTION_BAR_ACTIONS.map((item, index) => ({
    key: item.key as SelectionBarActionKey,
    enabled: true,
    order: index
  }));
  const custom = customAgents.map((agent, index) => ({
    key: `custom:${agent.id}` as SelectionBarActionKey,
    enabled: true,
    order: builtins.length + index
  }));
  return [...builtins, ...custom];
}

function normalizeCustomAgents(input: CustomAgent[]) {
  return input
    .map((item, index) => {
      const trimmedId = item.id?.trim() || `agent-${Date.now()}-${index}`;
      const fallbackName = `Agent ${index + 1}`;
      const normalizedName = trimNameByUnits(item.name?.trim() || fallbackName);
      return {
        ...item,
        id: trimmedId,
        name: normalizedName || fallbackName,
        icon: item.icon?.trim() || "Sparkles",
        prompt: item.prompt ?? "",
        enabled: true,
        order: index
      };
    })
    .slice(0, CUSTOM_AGENT_MAX_COUNT);
}

function normalizeSelectionBarOrder(
  input: SelectionBarItemConfig[] | undefined,
  customAgents: CustomAgent[]
) {
  const fallback = defaultSelectionBarOrder(customAgents);
  const inputItems = Array.isArray(input) ? [...input].sort((a, b) => a.order - b.order) : [];
  const customKeys = new Set(customAgents.map((item) => `custom:${item.id}`));
  const seen = new Set<string>();
  const normalized: SelectionBarItemConfig[] = [];

  for (const item of inputItems) {
    const key = String(item?.key ?? "").trim();
    if (!key || seen.has(key)) continue;
    if (!isBuiltinSelectionBarActionKey(key) && !customKeys.has(key)) continue;
    seen.add(key);
    normalized.push({
      key: key as SelectionBarActionKey,
      enabled: Boolean(item.enabled),
      order: normalized.length
    });
  }

  for (const item of fallback) {
    if (seen.has(item.key)) continue;
    normalized.push({
      key: item.key,
      enabled: item.enabled,
      order: normalized.length
    });
  }

  let enabledCount = 0;
  return normalized.map((item) => {
    if (!item.enabled) return item;
    if (enabledCount >= MAX_SELECTION_BAR_ENABLED_ITEMS) {
      return {
        ...item,
        enabled: false
      };
    }
    enabledCount += 1;
    return item;
  });
}

function validateAgentName(name: string) {
  const trimmed = name.trim();
  if (!trimmed) return "Agent 名称不能为空";
  if (countNameUnits(trimmed) > CUSTOM_AGENT_NAME_MAX_UNITS) {
    return "名称超出限制：最多 4 个中文字符或 8 个英文字符";
  }
  return null;
}

function makeDefaultCustomAgent(index: number): CustomAgent {
  return {
    id: `agent-${Date.now()}-${index}`,
    name: `Agent ${index + 1}`,
    icon: "Sparkles",
    prompt: CUSTOM_AGENT_PROMPT_TEMPLATE,
    enabled: true,
    order: index
  };
}

function clampPollMs(value: number) {
  if (!Number.isFinite(value)) return FALLBACK_SETTINGS.history.pollMs;
  return Math.min(POLL_MS_RANGE.max, Math.max(POLL_MS_RANGE.min, Math.round(value)));
}

function clampHistoryMax(value: number) {
  if (!Number.isFinite(value)) return FALLBACK_SETTINGS.history.maxItems;
  return Math.min(HISTORY_MAX_RANGE.max, Math.max(HISTORY_MAX_RANGE.min, Math.round(value)));
}

function parseFilter(value: unknown): FilterKind {
  return FILTER_OPTIONS.some((item) => item.key === value) ? (value as FilterKind) : "all";
}

function parseThemePreset(value: unknown): ThemePreset {
  if (THEME_OPTIONS.some((item) => item.key === value)) {
    return value as ThemePreset;
  }
  switch (value) {
    case "blue":
      return "blue";
    case "deep-black":
      return "deep-black";
    case "black":
    case "dark":
      return "deep-black";
    case "md2-dark":
    case "midnight":
      return "blue";
    case "graphite":
      return "gray";
    case "daylight":
    case "sunrise":
    case "amber-mist":
      return "white";
    default:
      return "deep-black";
  }
}

function readLegacyBoolean(
  input: Record<string, unknown>,
  key: string
): boolean | undefined {
  const value = input[key];
  return typeof value === "boolean" ? value : undefined;
}

function parsePasteBehavior(value: unknown): PasteBehavior {
  return PASTE_BEHAVIOR_OPTIONS.some((item) => item.key === value)
    ? (value as PasteBehavior)
    : "copy-and-hide";
}

function parseSelectionMode(value: unknown): SelectionTriggerMode {
  return SELECTION_MODE_OPTIONS.some((item) => item.key === value)
    ? (value as SelectionTriggerMode)
    : "auto-detect";
}

function parseTtsRuntimeMode(value: unknown): TtsRuntimeMode {
  return TTS_RUNTIME_MODE_OPTIONS.some((item) => item.key === value)
    ? (value as TtsRuntimeMode)
    : "dual-fallback";
}

function parseTranslateTarget(
  value: unknown,
  fallback: TranslateTargetLanguageCode = FALLBACK_SETTINGS.selectionAssistant.defaultTranslateTo
): TranslateTargetLanguageCode {
  const allowed: TranslateTargetLanguageCode[] = ["zh-CN", "en-US", "ja-JP", "ko-KR"];
  return allowed.includes(value as TranslateTargetLanguageCode)
    ? (value as TranslateTargetLanguageCode)
    : fallback;
}

function parseRequestId(value: string): bigint | null {
  const trimmed = value.trim();
  if (!trimmed || !/^\d+$/.test(trimmed)) return null;
  try {
    return BigInt(trimmed);
  } catch {
    return null;
  }
}

function isIncomingRequestFresh(currentId: string, incomingId: string): boolean {
  if (!incomingId) return false;
  if (!currentId) return true;
  const currentBigInt = parseRequestId(currentId);
  const incomingBigInt = parseRequestId(incomingId);
  if (currentBigInt !== null && incomingBigInt !== null) {
    return incomingBigInt >= currentBigInt;
  }
  return incomingId >= currentId;
}

function formatDate(iso: string) {
  const date = new Date(iso);
  return date.toLocaleTimeString([], {
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit"
  });
}

function snippet(content: string) {
  const compact = content.replace(/\s+/g, " ").trim();
  if (compact.length <= 108) return compact;
  return `${compact.slice(0, 108)}...`;
}

function formatBytes(bytes: number) {
  if (!Number.isFinite(bytes) || bytes <= 0) return "0 B";
  const units = ["B", "KB", "MB", "GB"];
  let value = bytes;
  let unitIndex = 0;
  while (value >= 1024 && unitIndex < units.length - 1) {
    value /= 1024;
    unitIndex += 1;
  }
  const fractionDigits = value >= 100 || unitIndex === 0 ? 0 : 1;
  return `${value.toFixed(fractionDigits)} ${units[unitIndex]}`;
}

function kindLabel(kind: ClipboardEntry["kind"]) {
  if (kind === "image") return "Image";
  if (kind === "link") return "Link";
  return "Text";
}

function isSameHistoryEntry(a: ClipboardEntry, b: ClipboardEntry) {
  if (
    a.id !== b.id ||
    a.kind !== b.kind ||
    a.copiedAt !== b.copiedAt ||
    a.pinned !== b.pinned
  ) {
    return false;
  }

  if (a.kind === "image" || b.kind === "image") {
    return (
      a.content === b.content &&
      (a.imageDataUrl?.length ?? 0) === (b.imageDataUrl?.length ?? 0)
    );
  }

  return a.content === b.content;
}

function isSameHistoryList(current: ClipboardEntry[], next: ClipboardEntry[]) {
  if (current === next) return true;
  if (current.length !== next.length) return false;
  for (let index = 0; index < current.length; index += 1) {
    if (!isSameHistoryEntry(current[index], next[index])) {
      return false;
    }
  }
  return true;
}

function getCurrentLabel() {
  try {
    return getCurrentWebviewWindow().label;
  } catch {
    return "main";
  }
}

function normalizeHotkeySegment(raw: string) {
  if (raw === " ") return "Space";
  if (raw === "ArrowUp") return "Up";
  if (raw === "ArrowDown") return "Down";
  if (raw === "ArrowLeft") return "Left";
  if (raw === "ArrowRight") return "Right";
  if (raw.length === 1) return raw.toUpperCase();
  return `${raw[0]?.toUpperCase() ?? ""}${raw.slice(1)}`;
}

function buildShortcutFromEvent(event: KeyboardEvent): string | null {
  if (["Control", "Shift", "Alt", "Meta"].includes(event.key)) {
    return null;
  }

  const parts: string[] = [];
  if (event.ctrlKey) parts.push("Ctrl");
  if (event.altKey) parts.push("Alt");
  if (event.shiftKey) parts.push("Shift");
  if (event.metaKey) parts.push("Meta");

  if (parts.length === 0) {
    return null;
  }

  parts.push(normalizeHotkeySegment(event.key));
  return parts.join("+");
}
interface SettingsApi {
  settings: AppSettings;
  loading: boolean;
  updating: boolean;
  error: string | null;
  updateSettings: (patch: AppSettingsPatch) => Promise<AppSettings | null>;
  setToggleShortcut: (shortcut: string) => Promise<AppSettings | null>;
  setToggleOcrShortcut: (shortcut: string) => Promise<AppSettings | null>;
  resetSettings: () => Promise<AppSettings | null>;
  exportSettings: () => Promise<string | null>;
  importSettings: (payload: string) => Promise<AppSettings | null>;
  refresh: () => Promise<void>;
}

function sanitizeSettings(input: AppSettings): AppSettings {
  const rawInput = input as unknown as Record<string, unknown>;
  const rawWindow =
    rawInput.window && typeof rawInput.window === "object" && !Array.isArray(rawInput.window)
      ? (rawInput.window as Record<string, unknown>)
      : null;
  const legacyLaunchOnSystemStartup =
    readLegacyBoolean(rawInput, "launchOnSystemStartup") ??
    (rawWindow ? readLegacyBoolean(rawWindow, "launchOnSystemStartup") : undefined);
  const legacySilentStartup =
    readLegacyBoolean(rawInput, "silentStartup") ??
    (rawWindow ? readLegacyBoolean(rawWindow, "silentStartup") : undefined);
  const legacyCheckUpdatesOnStartup =
    readLegacyBoolean(rawInput, "checkUpdatesOnStartup") ??
    (rawWindow ? readLegacyBoolean(rawWindow, "checkUpdatesOnStartup") : undefined);

  const sanitizedCustomAgents = normalizeCustomAgents(input.agents?.custom ?? []);
  const sanitizedBarOrder = normalizeSelectionBarOrder(
    input.agents?.barOrder,
    sanitizedCustomAgents
  );

  return {
    ...input,
    themePreset: parseThemePreset(input.themePreset),
    language: input.language === "en-US" ? "en-US" : "zh-CN",
    window: {
      autoHideOnBlur:
        input.window?.autoHideOnBlur ?? FALLBACK_SETTINGS.window.autoHideOnBlur,
      rememberPosition:
        input.window?.rememberPosition ?? FALLBACK_SETTINGS.window.rememberPosition,
      rememberMainWindowSize:
        input.window?.rememberMainWindowSize ??
        FALLBACK_SETTINGS.window.rememberMainWindowSize,
      launchOnSystemStartup:
        input.window?.launchOnSystemStartup ??
        legacyLaunchOnSystemStartup ??
        FALLBACK_SETTINGS.window.launchOnSystemStartup,
      silentStartup:
        input.window?.silentStartup ??
        legacySilentStartup ??
        FALLBACK_SETTINGS.window.silentStartup,
      checkUpdatesOnStartup:
        input.window?.checkUpdatesOnStartup ??
        legacyCheckUpdatesOnStartup ??
        FALLBACK_SETTINGS.window.checkUpdatesOnStartup
    },
    selectionAssistant: {
      enabled: Boolean(
        input.selectionAssistant?.enabled ?? FALLBACK_SETTINGS.selectionAssistant.enabled
      ),
      mode: parseSelectionMode(input.selectionAssistant?.mode),
      showIconAnimation: Boolean(
        input.selectionAssistant?.showIconAnimation ??
          FALLBACK_SETTINGS.selectionAssistant.showIconAnimation
      ),
      compactMode: Boolean(
        input.selectionAssistant?.compactMode ??
          FALLBACK_SETTINGS.selectionAssistant.compactMode
      ),
      autoHideMs: Math.min(
        30000,
        Math.max(
          800,
          (() => {
            const parsed =
              Number(
                input.selectionAssistant?.autoHideMs ??
                  FALLBACK_SETTINGS.selectionAssistant.autoHideMs
              ) || FALLBACK_SETTINGS.selectionAssistant.autoHideMs;
            return parsed === 3600 ? 5000 : parsed;
          })()
        )
      ),
      searchUrlTemplate:
        input.selectionAssistant?.searchUrlTemplate?.trim() ||
        FALLBACK_SETTINGS.selectionAssistant.searchUrlTemplate,
      minChars: Math.min(
        64,
        Math.max(
          1,
          Number(
            input.selectionAssistant?.minChars ??
              FALLBACK_SETTINGS.selectionAssistant.minChars
          ) || FALLBACK_SETTINGS.selectionAssistant.minChars
        )
      ),
      maxChars: Math.min(
        100000,
        Math.max(
          128,
          Number(
            input.selectionAssistant?.maxChars ??
              FALLBACK_SETTINGS.selectionAssistant.maxChars
          ) || FALLBACK_SETTINGS.selectionAssistant.maxChars
        )
      ),
      blockedApps: Array.isArray(input.selectionAssistant?.blockedApps)
        ? input.selectionAssistant.blockedApps
            .map((item) => item.trim().toLowerCase())
            .filter((item) => item.length > 0)
        : [],
      defaultTranslateTo: parseTranslateTarget(input.selectionAssistant?.defaultTranslateTo),
      resultWindowAlwaysOnTop: Boolean(
        input.selectionAssistant?.resultWindowAlwaysOnTop ??
          FALLBACK_SETTINGS.selectionAssistant.resultWindowAlwaysOnTop
      )
    },
    llm: {
      enabled: true,
      baseUrl: input.llm?.baseUrl?.trim() || FALLBACK_SETTINGS.llm.baseUrl,
      apiKey: input.llm?.apiKey ?? FALLBACK_SETTINGS.llm.apiKey,
      model: input.llm?.model?.trim() || FALLBACK_SETTINGS.llm.model,
      temperature: Math.min(
        2,
        Math.max(
          0,
          Number(input.llm?.temperature ?? FALLBACK_SETTINGS.llm.temperature) ||
            FALLBACK_SETTINGS.llm.temperature
        )
      ),
      maxTokens: Math.min(
        8192,
        Math.max(
          128,
          Number(input.llm?.maxTokens ?? FALLBACK_SETTINGS.llm.maxTokens) ||
            FALLBACK_SETTINGS.llm.maxTokens
        )
      ),
      timeoutMs: Math.min(
        120000,
        Math.max(
          5000,
          Number(input.llm?.timeoutMs ?? FALLBACK_SETTINGS.llm.timeoutMs) ||
            FALLBACK_SETTINGS.llm.timeoutMs
        )
      )
    },
    tts: {
      runtimeMode: parseTtsRuntimeMode(input.tts?.runtimeMode),
      voiceZhCn: input.tts?.voiceZhCn?.trim() || FALLBACK_SETTINGS.tts.voiceZhCn,
      voiceEnUs: input.tts?.voiceEnUs?.trim() || FALLBACK_SETTINGS.tts.voiceEnUs,
      ratePercent: Math.round(
        Math.min(
          TTS_RATE_PERCENT_RANGE.max,
          Math.max(
            TTS_RATE_PERCENT_RANGE.min,
            Number(input.tts?.ratePercent ?? FALLBACK_SETTINGS.tts.ratePercent) ||
              FALLBACK_SETTINGS.tts.ratePercent
          )
        )
      )
    },
    agents: {
      custom: sanitizedCustomAgents,
      barOrder: sanitizedBarOrder
    },
    shortcuts: {
      toggleMain: input.shortcuts?.toggleMain?.trim() || FALLBACK_SHORTCUT,
      toggleOcr: input.shortcuts?.toggleOcr?.trim() || FALLBACK_OCR_SHORTCUT
    },
    ocr: {
      enabled: Boolean(input.ocr?.enabled ?? FALLBACK_SETTINGS.ocr.enabled),
      autoRunAfterCapture: Boolean(
        input.ocr?.autoRunAfterCapture ?? FALLBACK_SETTINGS.ocr.autoRunAfterCapture
      ),
      defaultAction: OCR_ACTION_OPTIONS.some((item) => item.key === input.ocr?.defaultAction)
        ? (input.ocr?.defaultAction as OcrActionKind)
        : FALLBACK_SETTINGS.ocr.defaultAction,
      customAgentId: input.ocr?.customAgentId?.trim() || "",
      resultWindowAlwaysOnTop: Boolean(
        input.ocr?.resultWindowAlwaysOnTop ?? FALLBACK_SETTINGS.ocr.resultWindowAlwaysOnTop
      ),
      vision: {
        enabled: Boolean(
          (input.ocr?.vision?.apiKey ?? FALLBACK_SETTINGS.ocr.vision.apiKey).trim().length > 0
        ),
        baseUrl:
          input.ocr?.vision?.baseUrl?.trim() || FALLBACK_SETTINGS.ocr.vision.baseUrl,
        apiKey: input.ocr?.vision?.apiKey ?? FALLBACK_SETTINGS.ocr.vision.apiKey,
        model: input.ocr?.vision?.model?.trim() || FALLBACK_SETTINGS.ocr.vision.model,
        temperature: Math.min(
          2,
          Math.max(
            0,
            Number(input.ocr?.vision?.temperature ?? FALLBACK_SETTINGS.ocr.vision.temperature) ||
              FALLBACK_SETTINGS.ocr.vision.temperature
          )
        ),
        maxTokens: Math.min(
          OCR_VISION_MAX_TOKENS_RANGE.max,
          Math.max(
            OCR_VISION_MAX_TOKENS_RANGE.min,
            Number(input.ocr?.vision?.maxTokens ?? FALLBACK_SETTINGS.ocr.vision.maxTokens) ||
              FALLBACK_SETTINGS.ocr.vision.maxTokens
          )
        ),
        timeoutMs: Math.min(
          MODEL_TIMEOUT_MS_RANGE.max,
          Math.max(
            MODEL_TIMEOUT_MS_RANGE.min,
            Number(input.ocr?.vision?.timeoutMs ?? FALLBACK_SETTINGS.ocr.vision.timeoutMs) ||
              FALLBACK_SETTINGS.ocr.vision.timeoutMs
          )
        )
      }
    },
    history: {
      ...input.history,
      pollMs: clampPollMs(Number(input.history.pollMs)),
      maxItems: clampHistoryMax(Number(input.history.maxItems)),
      defaultCategory: parseFilter(input.history.defaultCategory),
      pasteBehavior: parsePasteBehavior(input.history.pasteBehavior),
      collapseTopBar: Boolean(
        input.history.collapseTopBar ?? FALLBACK_SETTINGS.history.collapseTopBar
      ),
      storagePath: input.history.storagePath?.trim() || ""
    }
  };
}

function mergeSettingsPatch<T>(base: T, patch: unknown): T {
  if (patch === undefined) {
    return base;
  }
  if (patch === null || typeof patch !== "object" || Array.isArray(patch)) {
    return patch as T;
  }
  const baseObject =
    base && typeof base === "object" && !Array.isArray(base)
      ? (base as Record<string, unknown>)
      : {};
  const result: Record<string, unknown> = { ...baseObject };
  for (const [key, value] of Object.entries(patch as Record<string, unknown>)) {
    if (value === undefined) continue;
    const current = baseObject[key];
    if (value !== null && typeof value === "object" && !Array.isArray(value)) {
      result[key] = mergeSettingsPatch(current, value);
    } else {
      result[key] = value;
    }
  }
  return result as T;
}

function sleepMs(ms: number) {
  return new Promise<void>((resolve) => {
    window.setTimeout(resolve, ms);
  });
}

function useAppSettings(): SettingsApi {
  const [settings, setSettings] = useState<AppSettings>(FALLBACK_SETTINGS);
  const [loading, setLoading] = useState(true);
  const [updating, setUpdating] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const settingsRef = useRef<AppSettings>(FALLBACK_SETTINGS);
  const updateTokenRef = useRef(0);
  const hasLoadedOnceRef = useRef(false);

  useEffect(() => {
    settingsRef.current = settings;
  }, [settings]);

  const refresh = useCallback(async () => {
    if (!hasLoadedOnceRef.current) {
      setLoading(true);
    }
    let lastError: unknown = null;
    try {
      for (let attempt = 0; attempt < 6; attempt += 1) {
        try {
          const remote = await invoke<AppSettings>("get_settings");
          const safe = sanitizeSettings(remote);
          settingsRef.current = safe;
          setSettings(safe);
          setError(null);
          hasLoadedOnceRef.current = true;
          return;
        } catch (invokeError) {
          lastError = invokeError;
          if (attempt < 5) {
            await sleepMs(220 + attempt * 130);
          }
        }
      }
      setError(String(lastError ?? "Failed to load settings"));
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  useEffect(() => {
    document.documentElement.dataset.theme = settings.themePreset;
    document.documentElement.lang = settings.language;
  }, [settings.language, settings.themePreset]);

  useEffect(() => {
    let active = true;
    let unlisten: (() => void) | null = null;
    try {
      void getCurrentWebviewWindow()
        .listen<boolean>("snapparse://settings-window-shown", () => {
          if (!active) return;
          void refresh();
        })
        .then((off) => {
          unlisten = off;
        });
    } catch {
      unlisten = null;
    }

    return () => {
      active = false;
      if (unlisten) {
        unlisten();
      }
    };
  }, [refresh]);

  useEffect(() => {
    let active = true;
    let promise: Promise<() => void> | null = null;

    try {
      promise = getCurrentWebviewWindow().listen<AppSettings>(
        SETTINGS_UPDATED_EVENT,
        (event) => {
          if (!active) return;
          const safe = sanitizeSettings(event.payload);
          settingsRef.current = safe;
          setSettings(safe);
        }
      );
    } catch {
      promise = null;
    }

    return () => {
      active = false;
      if (promise) {
        void promise.then((unlisten) => unlisten());
      }
    };
  }, []);

  const updateSettings = useCallback(async (patch: AppSettingsPatch) => {
    const previous = settingsRef.current;
    const optimistic = sanitizeSettings(mergeSettingsPatch(previous, patch));
    settingsRef.current = optimistic;
    setSettings(optimistic);
    setUpdating(true);
    const token = ++updateTokenRef.current;
    try {
      const remote = await invoke<AppSettings>("update_settings", { patch });
      const safe = sanitizeSettings(remote);
      if (token === updateTokenRef.current) {
        settingsRef.current = safe;
        setSettings(safe);
        setError(null);
      }
      return safe;
    } catch (invokeError) {
      if (token === updateTokenRef.current) {
        settingsRef.current = previous;
        setSettings(previous);
        setError(String(invokeError));
      }
      return null;
    } finally {
      if (token === updateTokenRef.current) {
        setUpdating(false);
      }
    }
  }, []);

  const setToggleShortcut = useCallback(async (shortcut: string) => {
    setUpdating(true);
    try {
      const remote = await invoke<AppSettings>("set_toggle_shortcut", { shortcut });
      const safe = sanitizeSettings(remote);
      setSettings(safe);
      setError(null);
      return safe;
    } catch (invokeError) {
      setError(String(invokeError));
      return null;
    } finally {
      setUpdating(false);
    }
  }, []);

  const setToggleOcrShortcut = useCallback(async (shortcut: string) => {
    setUpdating(true);
    try {
      const remote = await invoke<AppSettings>("set_toggle_ocr_shortcut", { shortcut });
      const safe = sanitizeSettings(remote);
      setSettings(safe);
      setError(null);
      return safe;
    } catch (invokeError) {
      setError(String(invokeError));
      return null;
    } finally {
      setUpdating(false);
    }
  }, []);

  const resetSettings = useCallback(async () => {
    setUpdating(true);
    try {
      const remote = await invoke<AppSettings>("reset_settings");
      const safe = sanitizeSettings(remote);
      setSettings(safe);
      setError(null);
      return safe;
    } catch (invokeError) {
      setError(String(invokeError));
      return null;
    } finally {
      setUpdating(false);
    }
  }, []);

  const exportSettings = useCallback(async () => {
    setUpdating(true);
    try {
      const payload = await invoke<string>("export_settings");
      setError(null);
      return payload;
    } catch (invokeError) {
      setError(String(invokeError));
      return null;
    } finally {
      setUpdating(false);
    }
  }, []);

  const importSettings = useCallback(async (payload: string) => {
    setUpdating(true);
    try {
      const remote = await invoke<AppSettings>("import_settings", { payload });
      const safe = sanitizeSettings(remote);
      setSettings(safe);
      setError(null);
      return safe;
    } catch (invokeError) {
      setError(String(invokeError));
      return null;
    } finally {
      setUpdating(false);
    }
  }, []);

  return {
    settings,
    loading,
    updating,
    error,
    updateSettings,
    setToggleShortcut,
    setToggleOcrShortcut,
    resetSettings,
    exportSettings,
    importSettings,
    refresh
  };
}
function ClipboardWindow({ settingsApi }: { settingsApi: SettingsApi }) {
  const { settings, error } = settingsApi;
  const [history, setHistory] = useState<ClipboardEntry[]>([]);
  const [query, setQuery] = useState("");
  const [filter, setFilter] = useState<FilterKind>(() => settings.history.defaultCategory);
  const [isPinnedTop, setIsPinnedTop] = useState(false);
  const [topControlsVisible, setTopControlsVisible] = useState(
    () => !settings.history.collapseTopBar
  );
  const collapseTopBarEnabled = settings.history.collapseTopBar;
  const showTopControls = topControlsVisible;
  const wheelDirectionRef = useRef<"up" | "down" | null>(null);
  const wheelDeltaRef = useRef(0);
  const wheelTsRef = useRef(0);
  const syncInFlightRef = useRef(false);
  const historyRef = useRef<ClipboardEntry[]>([]);
  const applyHistoryUpdate = useCallback((next: ClipboardEntry[]) => {
    setHistory((current) => (isSameHistoryList(current, next) ? current : next));
  }, []);

  useEffect(() => {
    historyRef.current = history;
  }, [history]);

  useEffect(() => {
    void invoke<boolean>("get_main_window_pinned_cmd")
      .then((value) => setIsPinnedTop(Boolean(value)))
      .catch(() => {
        void getCurrentWebviewWindow()
          .isAlwaysOnTop()
          .then((value) => setIsPinnedTop(value))
          .catch(() => {
            setIsPinnedTop(false);
          });
      });
  }, []);

  useEffect(() => {
    if (error) {
      console.error("[ClipboardWindow] settings error:", error);
    }
  }, [error]);

  useEffect(() => {
    setFilter(settings.history.defaultCategory);
  }, [settings.history.defaultCategory]);

  useEffect(() => {
    setTopControlsVisible(!collapseTopBarEnabled);
  }, [collapseTopBarEnabled]);

  useEffect(() => {
    if (!collapseTopBarEnabled) {
      return;
    }

    const handleFocus = () => {
      setTopControlsVisible(false);
    };
    window.addEventListener("focus", handleFocus);
    return () => {
      window.removeEventListener("focus", handleFocus);
    };
  }, [collapseTopBarEnabled]);

  useEffect(() => {
    wheelDirectionRef.current = null;
    wheelDeltaRef.current = 0;
    wheelTsRef.current = 0;
  }, [topControlsVisible]);

  async function loadHistory() {
    try {
      const items = await invoke<ClipboardEntry[]>("get_history");
      applyHistoryUpdate(items);
      return items;
    } catch (invokeError) {
      console.error("[ClipboardWindow] load_history failed:", invokeError);
      return null;
    }
  }

  async function syncClipboard() {
    if (syncInFlightRef.current) {
      return;
    }
    syncInFlightRef.current = true;
    try {
      const items = await invoke<ClipboardEntry[] | null>("sync_clipboard");
      if (items) {
        applyHistoryUpdate(items);
      }
    } catch {
      // Ignore transient clipboard conflicts from other apps.
    } finally {
      syncInFlightRef.current = false;
    }
  }

  useEffect(() => {
    let active = true;
    void (async () => {
      for (let attempt = 0; attempt < 12 && active; attempt += 1) {
        const loaded = await loadHistory();
        if ((loaded?.length ?? 0) > 0 || historyRef.current.length > 0) {
          break;
        }
        await syncClipboard();
        if (historyRef.current.length > 0) {
          break;
        }
        if (attempt < 11) {
          await sleepMs(180 + attempt * 90);
        }
      }
    })();

    return () => {
      active = false;
    };
  }, []);

  useEffect(() => {
    const timer = setInterval(() => {
      void syncClipboard();
    }, settings.history.pollMs);

    return () => clearInterval(timer);
  }, [settings.history.pollMs]);

  const counts = useMemo<Record<FilterKind, number>>(() => {
    const result: Record<FilterKind, number> = {
      all: history.length,
      text: 0,
      link: 0,
      image: 0,
      favorite: 0
    };

    for (const item of history) {
      if (item.kind === "text") result.text += 1;
      if (item.kind === "link") result.link += 1;
      if (item.kind === "image") result.image += 1;
      if (item.pinned) result.favorite += 1;
    }

    return result;
  }, [history]);

  const historySearchIndex = useMemo(
    () => history.map((item) => ({ item, normalizedContent: item.content.toLowerCase() })),
    [history]
  );
  const normalizedQuery = query.trim().toLowerCase();

  const filtered = useMemo(() => {
    let items: ClipboardEntry[];
    if (!normalizedQuery) {
      if (filter === "all") {
        return history;
      }
      if (filter === "favorite") {
        return history.filter((item) => item.pinned);
      }
      return history.filter((item) => item.kind === filter);
    }

    if (filter === "all") {
      items = historySearchIndex
        .filter((item) => item.normalizedContent.includes(normalizedQuery))
        .map((item) => item.item);
    } else if (filter === "favorite") {
      items = historySearchIndex
        .filter((item) => item.item.pinned && item.normalizedContent.includes(normalizedQuery))
        .map((item) => item.item);
    } else {
      items = historySearchIndex
        .filter(
          (item) =>
            item.item.kind === filter && item.normalizedContent.includes(normalizedQuery)
        )
        .map((item) => item.item);
    }

    return items;
  }, [filter, history, historySearchIndex, normalizedQuery]);

  function copyByClick(entry: ClipboardEntry) {
    return invoke<ClipboardEntry[]>("paste_entry_by_click", { id: entry.id })
      .then((items) => {
        applyHistoryUpdate(items);
      })
      .catch((invokeError) => {
        console.error("[ClipboardWindow] copy by click failed:", invokeError);
      });
  }

  async function toggleFavorite(id: string) {
    try {
      const items = await invoke<ClipboardEntry[]>("toggle_pin", { id });
      applyHistoryUpdate(items);
    } catch (invokeError) {
      console.error("[ClipboardWindow] toggle favorite failed:", invokeError);
    }
  }

  async function openSettingsWindow() {
    try {
      await invoke("show_settings_window_cmd");
    } catch (invokeError) {
      console.error("[ClipboardWindow] open settings failed:", invokeError);
    }
  }

  async function togglePinTop() {
    const next = !isPinnedTop;
    try {
      const pinned = await invoke<boolean>("set_main_window_pinned_cmd", {
        pinned: next
      });
      setIsPinnedTop(Boolean(pinned));
    } catch (invokeError) {
      console.error("[ClipboardWindow] pin toggle failed:", invokeError);
    }
  }

  function handlePanelDragMouseDown(event: React.MouseEvent<HTMLElement>) {
    const target = event.target as HTMLElement | null;
    if (target?.closest("button, input, select, textarea, a, [role='button']")) {
      return;
    }

    void invoke("start_main_window_drag_cmd").catch(() => {
      // Ignore drag failures outside Tauri runtime.
    });
  }

  function handleStreamWheel(event: ReactWheelEvent<HTMLElement>) {
    const deltaY = event.deltaY;
    if (Math.abs(deltaY) < 1) {
      return;
    }

    const direction: "up" | "down" = deltaY < 0 ? "up" : "down";
    const now = performance.now();

    if (wheelDirectionRef.current !== direction || now - wheelTsRef.current > 220) {
      wheelDeltaRef.current = 0;
    }
    wheelDirectionRef.current = direction;
    wheelTsRef.current = now;
    wheelDeltaRef.current += Math.abs(deltaY);

    const showThreshold = 42;
    const hideThreshold = 58;

    if (direction === "up" && !topControlsVisible && wheelDeltaRef.current >= showThreshold) {
      setTopControlsVisible(true);
      wheelDeltaRef.current = 0;
      return;
    }

    if (direction === "down" && topControlsVisible && wheelDeltaRef.current >= hideThreshold) {
      setTopControlsVisible(false);
      wheelDeltaRef.current = 0;
    }
  }

  return (
    <main className={`window-root clipboard-window${showTopControls ? "" : " top-controls-collapsed"}`}>
      <header className="panel-head" onMouseDown={handlePanelDragMouseDown}>
        <div>
          <h1 className="app-brand-gradient">SnapParse</h1>
        </div>
        <div className="head-actions">
          <button
            className={`icon-btn${isPinnedTop ? " active" : ""}`}
            onClick={() => void togglePinTop()}
            aria-label={isPinnedTop ? "Disable always on top" : "Enable always on top"}
          >
            {isPinnedTop ? <PinOff size={16} /> : <Pin size={16} />}
          </button>
          <button className="icon-btn" onClick={() => void openSettingsWindow()} aria-label="Open settings">
            <Settings size={16} />
          </button>
        </div>
      </header>

      <section
        className={`clipboard-top-controls${showTopControls ? " expanded" : " collapsed"}`}
        aria-hidden={!showTopControls}
      >
        <section className="toolbar">
          <label className="search-box" aria-label="Search clipboard history">
            <Search size={16} />
            <input
              value={query}
              onChange={(event) => setQuery(event.target.value)}
              placeholder="搜索文本、链接、图片..."
            />
          </label>
        </section>

        <section className="category-row" aria-label="Clipboard categories">
          {FILTER_OPTIONS.map((option) => {
            const Icon = option.icon;
            return (
              <button
                key={option.key}
                className={`category-chip${filter === option.key ? " active" : ""}`}
                onClick={() => setFilter(option.key)}
              >
                <Icon size={12} />
                <span>{option.label}</span>
                <small>{counts[option.key]}</small>
              </button>
            );
          })}
        </section>
      </section>

      <section className="clip-stream" aria-label="Clipboard history list" onWheel={handleStreamWheel}>
        {filtered.length === 0 && <div className="empty-note">暂无匹配内容</div>}

        {filtered.map((entry) => (
          <article
            key={entry.id}
            className={`clip-item kind-${entry.kind}${entry.pinned ? " pinned" : ""}`}
            onClick={() => void copyByClick(entry)}
            onKeyDown={(event) => {
              if (event.key === "Enter" || event.key === " ") {
                event.preventDefault();
                void copyByClick(entry);
              }
            }}
            role="button"
            tabIndex={0}
          >
            <header>
              <time>{formatDate(entry.copiedAt)}</time>
              <div className="clip-item-actions">
                <div className="tag">{kindLabel(entry.kind)}</div>
                <button
                  className={`favorite-btn${entry.pinned ? " active" : ""}`}
                  onClick={(event) => {
                    event.preventDefault();
                    event.stopPropagation();
                    void toggleFavorite(entry.id);
                  }}
                  aria-label={entry.pinned ? "取消收藏" : "收藏"}
                  title={entry.pinned ? "取消收藏" : "收藏"}
                >
                  <Star size={12} />
                </button>
              </div>
            </header>

            {entry.kind === "image" && entry.imageDataUrl ? (
              <img
                className="item-image"
                src={entry.imageDataUrl}
                alt={`Clipboard image preview ${entry.content}`}
                loading="lazy"
              />
            ) : (
              <pre className={entry.kind === "link" ? "link-text" : undefined}>{snippet(entry.content)}</pre>
            )}
          </article>
        ))}
      </section>
    </main>
  );
}

function iconForCustomAgent(name: string): LucideIcon {
  const lowered = name.trim().toLowerCase();
  const exact = CUSTOM_AGENT_ICON_MAP[lowered];
  if (exact) return exact;
  if (lowered.includes("search")) return Search;
  if (lowered.includes("translate")) return Languages;
  if (lowered.includes("explain")) return Info;
  if (lowered.includes("copy")) return Copy;
  if (lowered.includes("spark")) return Sparkles;
  return Bot;
}

function iconGlyphForCustomAgent(iconKey: string) {
  const key = iconKey.trim().toLowerCase();
  if (key.includes("spark")) return "✨";
  if (key.includes("bot")) return "🤖";
  if (key.includes("search")) return "🔎";
  if (key.includes("language")) return "🌐";
  if (key.includes("translate")) return "🌐";
  if (key.includes("copy")) return "📋";
  if (key.includes("star")) return "⭐";
  if (key.includes("light")) return "💡";
  if (key.includes("code")) return "💻";
  if (key.includes("book")) return "📘";
  if (key.includes("pen")) return "✏️";
  return "◆";
}

function SelectionBarWindow({ settingsApi }: { settingsApi: SettingsApi }) {
  const { settings } = settingsApi;
  const [selection, setSelection] = useState<SelectionDetectedPayload | null>(null);
  const [busyAction, setBusyAction] = useState<string | null>(null);
  type SelectionBarRenderAction =
    | {
        key: SelectionBarActionKey;
        kind: "builtin";
        meta: (typeof BUILTIN_SELECTION_BAR_ACTIONS)[number];
      }
    | {
        key: SelectionBarActionKey;
        kind: "custom";
        agent: CustomAgent;
      };

  useEffect(() => {
    let unlistenDetected: (() => void) | null = null;
    let unlistenError: (() => void) | null = null;

    void getCurrentWebviewWindow()
      .listen<SelectionDetectedPayload>("snapparse://selection-detected", (event) => {
        setSelection(event.payload);
      })
      .then((off) => {
        unlistenDetected = off;
      });

    void getCurrentWebviewWindow()
      .listen<string>("snapparse://selection-error", (event) => {
        console.error("[SelectionBarWindow] selection error:", event.payload);
      })
      .then((off) => {
        unlistenError = off;
      });

    return () => {
      if (unlistenDetected) unlistenDetected();
      if (unlistenError) unlistenError();
    };
  }, []);

  const customAgentMap = useMemo(() => {
    const map = new Map<string, CustomAgent>();
    for (const agent of settings.agents.custom) {
      map.set(agent.id, agent);
    }
    return map;
  }, [settings.agents.custom]);

  const orderedActions = useMemo<SelectionBarRenderAction[]>(() => {
    const order = normalizeSelectionBarOrder(settings.agents.barOrder, settings.agents.custom);
    const result: SelectionBarRenderAction[] = [];
    for (const item of order) {
      if (!item.enabled) continue;
      if (isBuiltinSelectionBarActionKey(item.key)) {
        result.push({
          key: item.key,
          kind: "builtin",
          meta: BUILTIN_SELECTION_BAR_ACTION_MAP[item.key]
        });
        continue;
      }
      const customId = parseCustomAgentActionKey(item.key);
      if (!customId) continue;
      const agent = customAgentMap.get(customId);
      if (!agent) continue;
      result.push({
        key: item.key,
        kind: "custom",
        agent
      });
    }
    return result;
  }, [customAgentMap, settings.agents.barOrder, settings.agents.custom]);

  async function copySelection() {
    if (!selection?.text) return;
    try {
      await invoke("copy_selection_text", { text: selection.text });
    } catch (invokeError) {
      console.error("[SelectionBarWindow] copy selection failed:", invokeError);
      try {
        await navigator.clipboard.writeText(selection.text);
      } catch (fallbackError) {
        console.error("[SelectionBarWindow] fallback copy failed:", fallbackError);
      }
    } finally {
      try {
        await invoke("hide_selection_bar");
      } catch (hideError) {
        console.error("[SelectionBarWindow] hide after copy failed:", hideError);
      }
    }
  }

  async function openSearch() {
    if (!selection?.text) return;
    await invoke("open_search_with_text", { text: selection.text });
  }

  async function runAction(action: SelectionActionKind, customAgentId?: string) {
    if (!selection?.text) return;
    const key = customAgentId ? `${action}:${customAgentId}` : action;
    const defaultTranslateTarget = settings.selectionAssistant.defaultTranslateTo;
    setBusyAction(key);
    try {
      await invoke<SelectionResultPayload>("run_selection_action", {
        payload: {
          action,
          text: selection.text,
          customAgentId,
          translateFrom: action === "translate" ? "auto" : null,
          translateTo: action === "translate" ? defaultTranslateTarget : null
        }
      });
    } catch (invokeError) {
      console.error("[SelectionBarWindow] run_selection_action failed:", invokeError);
    } finally {
      setBusyAction(null);
    }
  }

  return (
    <main className="window-root selection-bar-window">
      <section
        className={`selection-bar-shell${
          settings.selectionAssistant.compactMode ? " compact" : ""
        }`}
      >
        <div
          className={`selection-brand${
            settings.selectionAssistant.showIconAnimation ? " animate" : ""
          }`}
          title="SnapParse"
        >
          <img src={appLogo} alt="SnapParse" />
        </div>
        {orderedActions.map((item) => {
          if (item.kind === "builtin") {
            const meta = item.meta;
            const Icon = meta.icon;
            const busyKey = meta.action ?? null;
            const isBusy = Boolean(busyKey && busyAction === busyKey);
            return (
              <button
                key={item.key}
                className={`selection-action-btn${
                  settings.selectionAssistant.compactMode ? " compact" : ""
                }`}
                disabled={isBusy}
                onClick={() => {
                  if (meta.direct === "copy") {
                    void copySelection();
                    return;
                  }
                  if (meta.direct === "search") {
                    void openSearch();
                    return;
                  }
                  if (meta.action) {
                    void runAction(meta.action);
                  }
                }}
                title={meta.label}
              >
                <Icon size={13} />
                {!settings.selectionAssistant.compactMode && (
                  <span>{isBusy ? "处理中..." : meta.label}</span>
                )}
              </button>
            );
          }

          const Icon = iconForCustomAgent(item.agent.icon);
          const busyKey = `custom:${item.agent.id}`;
          const isBusy = busyAction === busyKey;
          return (
            <button
              key={item.key}
              className={`selection-action-btn custom${
                settings.selectionAssistant.compactMode ? " compact" : ""
              }`}
              disabled={isBusy}
              onClick={() => void runAction("custom", item.agent.id)}
              title={item.agent.name}
            >
              <Icon size={13} />
              {!settings.selectionAssistant.compactMode && (
                <span>{isBusy ? "处理中..." : item.agent.name}</span>
              )}
            </button>
          );
        })}
      </section>
    </main>
  );
}

function SelectionResultWindow({ settingsApi }: { settingsApi: SettingsApi }) {
  const { settings, updateSettings } = settingsApi;
  const [result, setResult] = useState<SelectionResultPayload | null>(null);
  const [isPinnedTop, setIsPinnedTop] = useState(
    () => settings.selectionAssistant.resultWindowAlwaysOnTop
  );
  const [fromLang, setFromLang] = useState<TranslateLanguageCode>("auto");
  const [toLang, setToLang] = useState<TranslateTargetLanguageCode>(
    settings.selectionAssistant.defaultTranslateTo
  );
  const [sourceExpanded, setSourceExpanded] = useState(false);
  const latestRequestIdRef = useRef("");
  const ttsAudioRef = useRef<HTMLAudioElement | null>(null);
  const ttsAudioRevokeRef = useRef<(() => void) | null>(null);
  const ttsRequestTokenRef = useRef(0);
  const suppressBlurStopUntilRef = useRef(0);
  const [ttsLoading, setTtsLoading] = useState(false);
  const [ttsPlaying, setTtsPlaying] = useState(false);
  const [outputFavorited, setOutputFavorited] = useState(false);

  useEffect(() => {
    void invoke<boolean>("get_result_window_pinned_cmd")
      .then((value) => setIsPinnedTop(Boolean(value)))
      .catch(() => {
        setIsPinnedTop(settings.selectionAssistant.resultWindowAlwaysOnTop);
      });
  }, [settings.selectionAssistant.resultWindowAlwaysOnTop]);

  useEffect(() => {
    let unlisten: (() => void) | null = null;
    void getCurrentWebviewWindow()
      .listen<SelectionResultPayload>("snapparse://selection-result-updated", (event) => {
        const payload = event.payload;
        if (!isIncomingRequestFresh(latestRequestIdRef.current, payload.requestId)) {
          return;
        }
        setResult((current) => {
          if (!current || current.requestId !== payload.requestId) {
            return payload;
          }
          if (!current.isStreaming && payload.isStreaming) {
            return current;
          }
          if (!current.isStreaming && !current.errorMessage && Boolean(payload.errorMessage)) {
            return current;
          }
          return payload;
        });
        if (latestRequestIdRef.current !== payload.requestId) {
          stopTtsPlayback(false);
          latestRequestIdRef.current = payload.requestId;
          setSourceExpanded(false);
          setFromLang((payload.translateFrom as TranslateLanguageCode) || "auto");
          setToLang(parseTranslateTarget(payload.translateTo, settings.selectionAssistant.defaultTranslateTo));
          setOutputFavorited(false);
        }
      })
      .then((off) => {
        unlisten = off;
      });
    return () => {
      if (unlisten) unlisten();
    };
  }, []);

  async function togglePinTop() {
    const next = !isPinnedTop;
    try {
      const value = await invoke<boolean>("set_result_window_pinned_cmd", { pinned: next });
      setIsPinnedTop(Boolean(value));
    } catch (invokeError) {
      console.error("[SelectionResultWindow] pin toggle failed:", invokeError);
    }
  }

  async function minimizeResultWindow() {
    try {
      await invoke("minimize_selection_result_window");
    } catch (invokeError) {
      console.error("[SelectionResultWindow] minimize failed:", invokeError);
    }
  }

  async function closeResultWindow() {
    stopTtsPlayback();
    try {
      await invoke("close_selection_result_window");
    } catch (invokeError) {
      console.error("[SelectionResultWindow] hide failed:", invokeError);
    }
  }

  async function copyOutput() {
    if (!result?.outputText) return;
    try {
      await navigator.clipboard.writeText(result.outputText);
    } catch (error) {
      console.error("[SelectionResultWindow] copy output failed:", error);
    }
  }

  async function addFavoriteText(value: string) {
    const text = value.trim();
    if (!text) return;
    try {
      const pinned = await invoke<boolean>("toggle_favorite_text_cmd", { text });
      setOutputFavorited(Boolean(pinned));
    } catch (error) {
      console.error("[SelectionResultWindow] add favorite failed:", error);
      window.alert(`收藏失败：${String(error)}`);
    }
  }

  async function persistTranslateTarget(nextTo: TranslateTargetLanguageCode) {
    if (settings.selectionAssistant.defaultTranslateTo === nextTo) return;
    await updateSettings({
      selectionAssistant: {
        defaultTranslateTo: nextTo
      }
    });
  }

  async function rerunTranslate(nextFrom: TranslateLanguageCode, nextTo: TranslateTargetLanguageCode) {
    if (!result?.sourceText) return;
    try {
      await invoke<SelectionResultPayload>("run_selection_action", {
        payload: {
          action: "translate",
          text: result.sourceText,
          translateFrom: nextFrom,
          translateTo: nextTo
        }
      });
    } catch (invokeError) {
      console.error("[SelectionResultWindow] rerun translate failed:", invokeError);
    }
  }

  const stopTtsPlayback = useCallback((bumpToken = true) => {
    if (bumpToken) {
      ttsRequestTokenRef.current += 1;
    }
    const audio = ttsAudioRef.current;
    if (audio) {
      audio.onended = null;
      audio.onerror = null;
      audio.onpause = null;
      audio.pause();
      try {
        audio.currentTime = 0;
      } catch {
        // ignore
      }
      ttsAudioRef.current = null;
    }
    const revoke = ttsAudioRevokeRef.current;
    if (revoke) {
      revoke();
      ttsAudioRevokeRef.current = null;
    }
    setTtsLoading(false);
    setTtsPlaying(false);
  }, []);

  useEffect(() => {
    return () => {
      stopTtsPlayback();
    };
  }, [stopTtsPlayback]);

  useEffect(() => {
    let blurTimer: number | null = null;
    const verifyBlurAndStop = async () => {
      if (Date.now() < suppressBlurStopUntilRef.current) return;
      let focused = true;
      let visible = true;
      try {
        focused = await getCurrentWebviewWindow().isFocused();
      } catch {
        focused = document.hasFocus();
      }
      try {
        visible = await getCurrentWebviewWindow().isVisible();
      } catch {
        visible = true;
      }
      if (!focused || !visible || document.hidden) {
        stopTtsPlayback();
      }
    };
    const onBlur = () => {
      if (Date.now() < suppressBlurStopUntilRef.current) return;
      if (blurTimer !== null) {
        window.clearTimeout(blurTimer);
      }
      blurTimer = window.setTimeout(() => {
        void verifyBlurAndStop();
      }, 180);
    };
    const onFocus = () => {
      if (blurTimer !== null) {
        window.clearTimeout(blurTimer);
        blurTimer = null;
      }
    };
    const onMouseUp = () => {
      if (suppressBlurStopUntilRef.current > 0) {
        suppressBlurStopUntilRef.current = Date.now() + 180;
      }
    };
    const onVisibilityChange = () => {
      if (document.hidden) {
        stopTtsPlayback();
      }
    };
    window.addEventListener("blur", onBlur);
    window.addEventListener("focus", onFocus);
    window.addEventListener("mouseup", onMouseUp);
    document.addEventListener("visibilitychange", onVisibilityChange);
    return () => {
      if (blurTimer !== null) {
        window.clearTimeout(blurTimer);
      }
      window.removeEventListener("blur", onBlur);
      window.removeEventListener("focus", onFocus);
      window.removeEventListener("mouseup", onMouseUp);
      document.removeEventListener("visibilitychange", onVisibilityChange);
    };
  }, [stopTtsPlayback]);

  useEffect(() => {
    if (!ttsPlaying && !ttsLoading) return;
    let disposed = false;
    const timer = window.setInterval(() => {
      void getCurrentWebviewWindow()
        .isVisible()
        .then((visible) => {
          if (!visible && !disposed) {
            stopTtsPlayback();
          }
        })
        .catch(() => {
          if (document.hidden && !disposed) {
            stopTtsPlayback();
          }
        });
    }, 420);
    return () => {
      disposed = true;
      window.clearInterval(timer);
    };
  }, [ttsLoading, ttsPlaying, stopTtsPlayback]);

  async function speakOutput() {
    const text = result?.outputText?.trim() || "";
    if (!text) return;

    if (ttsPlaying || ttsLoading) {
      stopTtsPlayback();
      return;
    }

    const token = ttsRequestTokenRef.current + 1;
    ttsRequestTokenRef.current = token;
    setTtsLoading(true);
    setTtsPlaying(false);

    try {
      const response = await invoke<TtsSynthesizeResult>("synthesize_tts_cmd", {
        payload: {
          text,
          languageHint: settings.language
        }
      });

      if (token !== ttsRequestTokenRef.current) return;

      stopTtsPlayback(false);
      const { audio, revoke } = createAudioFromTtsResponse(response);
      ttsAudioRef.current = audio;
      ttsAudioRevokeRef.current = revoke;

      audio.onended = () => {
        if (token !== ttsRequestTokenRef.current) return;
        stopTtsPlayback(false);
      };
      audio.onpause = () => {
        if (token !== ttsRequestTokenRef.current) return;
        stopTtsPlayback(false);
      };
      audio.onerror = () => {
        if (token !== ttsRequestTokenRef.current) return;
        stopTtsPlayback(false);
      };

      await audio.play();
      if (token !== ttsRequestTokenRef.current) {
        audio.pause();
        return;
      }
      setTtsLoading(false);
      setTtsPlaying(true);
    } catch (error) {
      if (token !== ttsRequestTokenRef.current) return;
      setTtsLoading(false);
      setTtsPlaying(false);
      console.error("[SelectionResultWindow] TTS playback failed:", error);
      window.alert(`语音播放失败：${String(error)}`);
    }
  }

  const outputDisplayText =
    result?.errorMessage?.trim() ||
    result?.outputText?.trim() ||
    (result?.isStreaming ? "正在处理..." : "等待处理结果...");
  const outputSpeakableText = result?.outputText?.trim() || "";
  const outputFavoriteText = result?.outputText?.trim() || "";
  const resultWindowMeta = useMemo<{ label: string; icon: LucideIcon }>(() => {
    switch (result?.action) {
      case "translate":
        return { label: "Translator", icon: Languages };
      case "summary":
        return { label: "Summary", icon: ClipboardList };
      case "polish":
        return { label: "Polish", icon: Sparkles };
      case "explain":
        return { label: "Explain", icon: Info };
      case "custom":
        return {
          label: result?.customAgentName || "Custom Agent",
          icon: iconForCustomAgent(result?.customAgentIcon || result?.customAgentName || "Bot")
        };
      default:
        return { label: "Processor", icon: ScanSearch };
    }
  }, [result?.action, result?.customAgentName, result?.customAgentIcon]);
  const ResultWindowIcon = resultWindowMeta.icon;
  const markResultWindowDragging = () => {
    suppressBlurStopUntilRef.current = Date.now() + 3000;
  };

  return (
    <main className="window-root selection-result-window">
      <section className="selection-result-shell">
        <header
          className="selection-result-titlebar"
          data-tauri-drag-region
          onMouseDownCapture={markResultWindowDragging}
        >
          <div className="selection-result-meta">
            <ResultWindowIcon size={13} />
            <span>{resultWindowMeta.label}</span>
          </div>
          <div className="selection-result-window-controls">
            <button className="icon-btn" onClick={() => void minimizeResultWindow()} aria-label="Minimize">
              <Minus size={14} />
            </button>
            <button
              className={`icon-btn${isPinnedTop ? " active" : ""}`}
              onClick={() => void togglePinTop()}
              aria-label={isPinnedTop ? "Disable always on top" : "Enable always on top"}
            >
              {isPinnedTop ? <PinOff size={14} /> : <Pin size={14} />}
            </button>
            <button className="icon-btn" onClick={() => void closeResultWindow()} aria-label="Close">
              <X size={14} />
            </button>
          </div>
        </header>

        <section className="selection-result-content">
          {result?.action === "translate" && (
            <section className="selection-translate-row">
              <div className="selection-translate-item">
                <span className="selection-translate-label">默认语言 --&gt;</span>
                <select
                  className="selection-translate-select md2-select"
                  id="translate-from"
                  aria-label="默认语言"
                  disabled={Boolean(result?.isStreaming)}
                  value={fromLang}
                  onChange={(event) => {
                    const next = event.target.value as TranslateLanguageCode;
                    setFromLang(next);
                    void rerunTranslate(next, toLang);
                  }}
                >
                  {TRANSLATE_LANGUAGE_OPTIONS.map((item) => (
                    <option key={`from-${item.key}`} value={item.key}>
                      {item.label}
                    </option>
                  ))}
                </select>
              </div>
              <div className="selection-translate-item">
                <span className="selection-translate-label">目标语言 --&gt;</span>
                <select
                  className="selection-translate-select md2-select"
                  id="translate-to"
                  aria-label="目标语言"
                  disabled={Boolean(result?.isStreaming)}
                  value={toLang}
                  onChange={(event) => {
                    const next = parseTranslateTarget(event.target.value);
                    setToLang(next);
                    void persistTranslateTarget(next);
                    void rerunTranslate(fromLang, next);
                  }}
                >
                  {TRANSLATE_LANGUAGE_OPTIONS.filter((item) => item.key !== "auto").map((item) => (
                    <option key={`to-${item.key}`} value={item.key}>
                      {item.label}
                    </option>
                  ))}
                </select>
              </div>
            </section>
          )}

          <section className="selection-result-body">
            <section className="selection-result-output">
              <pre className="selection-result-text">{outputDisplayText}</pre>
              <div className="result-overlay-actions">
                <button
                  className={`icon-btn overlay-action-btn result-favorite-btn${
                    outputFavorited ? " active" : ""
                  }`}
                  onClick={() => void addFavoriteText(outputFavoriteText)}
                  aria-label="Favorite result"
                  title="收藏"
                  disabled={!outputFavoriteText}
                >
                  <Star size={14} />
                </button>
                <button
                  className={`icon-btn overlay-action-btn result-tts-btn${
                    ttsPlaying ? " active" : ""
                  }`}
                  onClick={() => void speakOutput()}
                  aria-label={ttsPlaying ? "Stop speech" : "Play speech"}
                  title={ttsPlaying ? "停止语音" : "语音播放"}
                  disabled={!outputSpeakableText || ttsLoading}
                >
                  <Volume2 size={14} />
                </button>
                <button
                  className="icon-btn overlay-action-btn result-copy-btn"
                  onClick={() => void copyOutput()}
                  aria-label="Copy result"
                >
                  <Copy size={14} />
                </button>
              </div>
            </section>

            <section className={`selection-source-fold${sourceExpanded ? " open" : ""}`}>
              {sourceExpanded && (
                <pre className="selection-source-text">
                  {result?.sourceText || "暂无内容"}
                </pre>
              )}
              <button
                type="button"
                className="selection-source-toggle overlay-action-btn"
                aria-label={sourceExpanded ? "Collapse source text" : "Expand source text"}
                title={sourceExpanded ? "Collapse source text" : "Expand source text"}
                onClick={() => setSourceExpanded((value) => !value)}
              >
                <ChevronsUpDown size={14} />
              </button>
            </section>
          </section>
        </section>
      </section>
    </main>
  );
}

function OcrCaptureWindow() {
  const [startPoint, setStartPoint] = useState<{ x: number; y: number } | null>(null);
  const [currentPoint, setCurrentPoint] = useState<{ x: number; y: number } | null>(null);
  const [submitting, setSubmitting] = useState(false);
  const submittingRef = useRef(false);

  const selectionRect = useMemo(() => {
    if (!startPoint || !currentPoint) return null;
    const left = Math.min(startPoint.x, currentPoint.x);
    const top = Math.min(startPoint.y, currentPoint.y);
    const width = Math.abs(startPoint.x - currentPoint.x);
    const height = Math.abs(startPoint.y - currentPoint.y);
    return { left, top, width, height };
  }, [currentPoint, startPoint]);

  const resetSelection = useCallback(() => {
    setStartPoint(null);
    setCurrentPoint(null);
  }, []);

  const cancelCapture = useCallback(async () => {
    resetSelection();
    submittingRef.current = false;
    setSubmitting(false);
    try {
      await invoke("cancel_ocr_capture_cmd");
    } catch (invokeError) {
      console.error("[OcrCaptureWindow] cancel capture failed:", invokeError);
    }
  }, [resetSelection]);

  const submitCapture = useCallback(
    (rect: { left: number; top: number; width: number; height: number }) => {
      if (submittingRef.current) return;
      if (rect.width < 4 || rect.height < 4) {
        resetSelection();
        return;
      }

      submittingRef.current = true;
      setSubmitting(true);
      resetSelection();

      void invoke("complete_ocr_capture_cmd", {
        area: {
          x: rect.left,
          y: rect.top,
          width: rect.width,
          height: rect.height
        }
      })
        .catch(async (invokeError) => {
          console.error("[OcrCaptureWindow] complete capture failed:", invokeError);
          try {
            await invoke("cancel_ocr_capture_cmd");
          } catch {
            // ignore
          }
        })
        .finally(() => {
          submittingRef.current = false;
          setSubmitting(false);
        });
    },
    [resetSelection]
  );

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === "Escape") {
        event.preventDefault();
        void cancelCapture();
      }
    };
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, [cancelCapture]);

  useEffect(() => {
    let offStarted: (() => void) | null = null;
    let offCanceled: (() => void) | null = null;
    void getCurrentWebviewWindow()
      .listen("snapparse://ocr-capture-started", () => {
        resetSelection();
        submittingRef.current = false;
        setSubmitting(false);
      })
      .then((off) => {
        offStarted = off;
      });
    void getCurrentWebviewWindow()
      .listen("snapparse://ocr-capture-canceled", () => {
        resetSelection();
        submittingRef.current = false;
        setSubmitting(false);
      })
      .then((off) => {
        offCanceled = off;
      });
    return () => {
      if (offStarted) offStarted();
      if (offCanceled) offCanceled();
    };
  }, [resetSelection]);

  return (
    <main
      className="window-root ocr-capture-window"
      onContextMenu={(event) => event.preventDefault()}
      onMouseDown={(event) => {
        if (event.button !== 0 || submitting) return;
        const point = { x: event.clientX, y: event.clientY };
        setStartPoint(point);
        setCurrentPoint(point);
      }}
      onMouseMove={(event) => {
        if (!startPoint || submitting) return;
        setCurrentPoint({ x: event.clientX, y: event.clientY });
      }}
      onMouseUp={() => {
        if (!selectionRect || submitting) return;
        void submitCapture(selectionRect);
      }}
    >
      <div className="ocr-capture-hint">
        <span>{submitting ? "识别中..." : "拖动鼠标框选区域，松开后开始 OCR（Esc 取消）"}</span>
      </div>
      {selectionRect && (
        <div
          className="ocr-capture-rect"
          style={{
            left: `${selectionRect.left}px`,
            top: `${selectionRect.top}px`,
            width: `${selectionRect.width}px`,
            height: `${selectionRect.height}px`
          }}
        />
      )}
    </main>
  );
}

function OcrResultWindow({ settingsApi }: { settingsApi: SettingsApi }) {
  const { settings } = settingsApi;
  const [result, setResult] = useState<OcrResultPayload | null>(null);
  const [isPinnedTop, setIsPinnedTop] = useState(() => settings.ocr.resultWindowAlwaysOnTop);
  const latestRequestIdRef = useRef("");
  const ttsAudioRef = useRef<HTMLAudioElement | null>(null);
  const ttsAudioRevokeRef = useRef<(() => void) | null>(null);
  const ttsRequestTokenRef = useRef(0);
  const suppressBlurStopUntilRef = useRef(0);
  const [ttsLoadingPanel, setTtsLoadingPanel] = useState<"ocr" | "output" | null>(null);
  const [ttsPlayingPanel, setTtsPlayingPanel] = useState<"ocr" | "output" | null>(null);
  const [ocrFavorited, setOcrFavorited] = useState(false);
  const [outputFavorited, setOutputFavorited] = useState(false);

  useEffect(() => {
    void invoke<boolean>("get_ocr_result_window_pinned_cmd")
      .then((value) => setIsPinnedTop(Boolean(value)))
      .catch(() => {
        setIsPinnedTop(settings.ocr.resultWindowAlwaysOnTop);
      });
  }, [settings.ocr.resultWindowAlwaysOnTop]);

  useEffect(() => {
    let unlisten: (() => void) | null = null;
    void getCurrentWebviewWindow()
      .listen<OcrResultPayload>("snapparse://ocr-result-updated", (event) => {
        const payload = event.payload;
        if (!isIncomingRequestFresh(latestRequestIdRef.current, payload.requestId)) {
          return;
        }
        setResult((current) => {
          if (!current || current.requestId !== payload.requestId) {
            return payload;
          }
          if (!current.isStreaming && payload.isStreaming) {
            return current;
          }
          if (!current.isStreaming && !current.errorMessage && Boolean(payload.errorMessage)) {
            return current;
          }
          return payload;
        });
        if (latestRequestIdRef.current !== payload.requestId) {
          stopTtsPlayback(false);
          latestRequestIdRef.current = payload.requestId;
          setOcrFavorited(false);
          setOutputFavorited(false);
        }
      })
      .then((off) => {
        unlisten = off;
      });
    return () => {
      if (unlisten) unlisten();
    };
  }, []);

  const resultMeta = useMemo<{ label: string; icon: LucideIcon }>(() => {
    switch (result?.action) {
      case "translate":
        return { label: "Translator", icon: Languages };
      case "summary":
        return { label: "Summary", icon: ClipboardList };
      case "polish":
        return { label: "Polish", icon: Sparkles };
      case "explain":
        return { label: "Explain", icon: Info };
      case "custom":
        return {
          label: result?.customAgentName || "Custom Agent",
          icon: iconForCustomAgent(result?.customAgentIcon || result?.customAgentName || "Bot")
        };
      default:
        return { label: "OCR Processor", icon: ScanSearch };
    }
  }, [result?.action, result?.customAgentName, result?.customAgentIcon]);
  const MetaIcon = resultMeta.icon;

  async function togglePinTop() {
    const next = !isPinnedTop;
    try {
      const value = await invoke<boolean>("set_ocr_result_window_pinned_cmd", { pinned: next });
      setIsPinnedTop(Boolean(value));
    } catch (invokeError) {
      console.error("[OcrResultWindow] pin toggle failed:", invokeError);
    }
  }

  async function minimizeWindow() {
    try {
      await invoke("minimize_ocr_result_window_cmd");
    } catch (invokeError) {
      console.error("[OcrResultWindow] minimize failed:", invokeError);
    }
  }

  async function closeWindow() {
    stopTtsPlayback();
    try {
      await invoke("close_ocr_result_window_cmd");
    } catch (invokeError) {
      console.error("[OcrResultWindow] close failed:", invokeError);
    }
  }

  async function copyText(value: string) {
    const text = value.trim();
    if (!text) return;
    try {
      await navigator.clipboard.writeText(text);
    } catch (copyError) {
      console.error("[OcrResultWindow] copy failed:", copyError);
    }
  }

  async function addFavoriteText(value: string, panel: "ocr" | "output") {
    const text = value.trim();
    if (!text) return;
    try {
      const pinned = await invoke<boolean>("toggle_favorite_text_cmd", { text });
      if (panel === "ocr") {
        setOcrFavorited(Boolean(pinned));
      } else {
        setOutputFavorited(Boolean(pinned));
      }
    } catch (error) {
      console.error("[OcrResultWindow] add favorite failed:", error);
      window.alert(`收藏失败：${String(error)}`);
    }
  }

  const stopTtsPlayback = useCallback((bumpToken = true) => {
    if (bumpToken) {
      ttsRequestTokenRef.current += 1;
    }
    const audio = ttsAudioRef.current;
    if (audio) {
      audio.onended = null;
      audio.onerror = null;
      audio.onpause = null;
      audio.pause();
      try {
        audio.currentTime = 0;
      } catch {
        // ignore
      }
      ttsAudioRef.current = null;
    }
    const revoke = ttsAudioRevokeRef.current;
    if (revoke) {
      revoke();
      ttsAudioRevokeRef.current = null;
    }
    setTtsLoadingPanel(null);
    setTtsPlayingPanel(null);
  }, []);

  useEffect(() => {
    return () => {
      stopTtsPlayback();
    };
  }, [stopTtsPlayback]);

  useEffect(() => {
    let blurTimer: number | null = null;
    const verifyBlurAndStop = async () => {
      if (Date.now() < suppressBlurStopUntilRef.current) return;
      let focused = true;
      let visible = true;
      try {
        focused = await getCurrentWebviewWindow().isFocused();
      } catch {
        focused = document.hasFocus();
      }
      try {
        visible = await getCurrentWebviewWindow().isVisible();
      } catch {
        visible = true;
      }
      if (!focused || !visible || document.hidden) {
        stopTtsPlayback();
      }
    };
    const onBlur = () => {
      if (Date.now() < suppressBlurStopUntilRef.current) return;
      if (blurTimer !== null) {
        window.clearTimeout(blurTimer);
      }
      blurTimer = window.setTimeout(() => {
        void verifyBlurAndStop();
      }, 180);
    };
    const onFocus = () => {
      if (blurTimer !== null) {
        window.clearTimeout(blurTimer);
        blurTimer = null;
      }
    };
    const onMouseUp = () => {
      if (suppressBlurStopUntilRef.current > 0) {
        suppressBlurStopUntilRef.current = Date.now() + 180;
      }
    };
    const onVisibilityChange = () => {
      if (document.hidden) {
        stopTtsPlayback();
      }
    };
    window.addEventListener("blur", onBlur);
    window.addEventListener("focus", onFocus);
    window.addEventListener("mouseup", onMouseUp);
    document.addEventListener("visibilitychange", onVisibilityChange);
    return () => {
      if (blurTimer !== null) {
        window.clearTimeout(blurTimer);
      }
      window.removeEventListener("blur", onBlur);
      window.removeEventListener("focus", onFocus);
      window.removeEventListener("mouseup", onMouseUp);
      document.removeEventListener("visibilitychange", onVisibilityChange);
    };
  }, [stopTtsPlayback]);

  useEffect(() => {
    if (!ttsPlayingPanel && !ttsLoadingPanel) return;
    let disposed = false;
    const timer = window.setInterval(() => {
      void getCurrentWebviewWindow()
        .isVisible()
        .then((visible) => {
          if (!visible && !disposed) {
            stopTtsPlayback();
          }
        })
        .catch(() => {
          if (document.hidden && !disposed) {
            stopTtsPlayback();
          }
        });
    }, 420);
    return () => {
      disposed = true;
      window.clearInterval(timer);
    };
  }, [ttsLoadingPanel, ttsPlayingPanel, stopTtsPlayback]);

  async function speakPanel(panel: "ocr" | "output", value: string) {
    const text = value.trim();
    if (!text) return;

    if (ttsPlayingPanel === panel || ttsLoadingPanel === panel) {
      stopTtsPlayback();
      return;
    }

    const token = ttsRequestTokenRef.current + 1;
    ttsRequestTokenRef.current = token;
    setTtsLoadingPanel(panel);
    setTtsPlayingPanel(null);

    try {
      const response = await invoke<TtsSynthesizeResult>("synthesize_tts_cmd", {
        payload: {
          text,
          languageHint: settings.language
        }
      });

      if (token !== ttsRequestTokenRef.current) return;

      stopTtsPlayback(false);
      const { audio, revoke } = createAudioFromTtsResponse(response);
      ttsAudioRef.current = audio;
      ttsAudioRevokeRef.current = revoke;

      audio.onended = () => {
        if (token !== ttsRequestTokenRef.current) return;
        stopTtsPlayback(false);
      };
      audio.onpause = () => {
        if (token !== ttsRequestTokenRef.current) return;
        stopTtsPlayback(false);
      };
      audio.onerror = () => {
        if (token !== ttsRequestTokenRef.current) return;
        stopTtsPlayback(false);
      };

      await audio.play();
      if (token !== ttsRequestTokenRef.current) {
        audio.pause();
        return;
      }
      setTtsLoadingPanel(null);
      setTtsPlayingPanel(panel);
    } catch (error) {
      if (token !== ttsRequestTokenRef.current) return;
      setTtsLoadingPanel(null);
      setTtsPlayingPanel(null);
      console.error("[OcrResultWindow] TTS playback failed:", error);
      window.alert(`语音播放失败：${String(error)}`);
    }
  }

  const outputText =
    result?.errorMessage?.trim() ||
    result?.outputText?.trim() ||
    (result?.isStreaming ? "正在处理..." : "等待 OCR 处理结果...");
  const ocrSpeakableText = result?.ocrText?.trim() || "";
  const outputSpeakableText = result?.outputText?.trim() || "";
  const ocrFavoriteText = result?.ocrText?.trim() || "";
  const outputFavoriteText = result?.outputText?.trim() || "";
  const markOcrResultWindowDragging = () => {
    suppressBlurStopUntilRef.current = Date.now() + 3000;
  };

  return (
    <main className="window-root ocr-result-window">
      <section className="ocr-result-shell">
        <header
          className="ocr-result-titlebar"
          data-tauri-drag-region
          onMouseDownCapture={markOcrResultWindowDragging}
        >
          <div className="ocr-result-meta">
            <MetaIcon size={13} />
            <span>{resultMeta.label}</span>
          </div>
          <div className="ocr-result-window-controls">
            <button
              className={`icon-btn${isPinnedTop ? " active" : ""}`}
              onClick={() => void togglePinTop()}
              aria-label={isPinnedTop ? "Disable always on top" : "Enable always on top"}
            >
              {isPinnedTop ? <PinOff size={14} /> : <Pin size={14} />}
            </button>
            <button className="icon-btn" onClick={() => void minimizeWindow()} aria-label="Minimize">
              <Minus size={14} />
            </button>
            <button className="icon-btn" onClick={() => void closeWindow()} aria-label="Close">
              <X size={14} />
            </button>
          </div>
        </header>

        <section className="ocr-result-content">
          <article className="ocr-result-block">
            <header>
              <span>OCR Text</span>
              <div className="ocr-block-actions">
                <button
                  className={`icon-btn ocr-block-favorite-btn${ocrFavorited ? " active" : ""}`}
                  onClick={() => void addFavoriteText(ocrFavoriteText, "ocr")}
                  aria-label="Favorite OCR text"
                  title="收藏"
                  disabled={!ocrFavoriteText}
                >
                  <Star size={14} />
                </button>
                <button
                  className={`icon-btn ocr-block-tts-btn${
                    ttsPlayingPanel === "ocr" ? " active" : ""
                  }`}
                  onClick={() => void speakPanel("ocr", result?.ocrText || "")}
                  aria-label={ttsPlayingPanel === "ocr" ? "Stop speech" : "Play OCR text"}
                  title={ttsPlayingPanel === "ocr" ? "停止语音" : "语音播放"}
                  disabled={!ocrSpeakableText || ttsLoadingPanel === "ocr"}
                >
                  <Volume2 size={14} />
                </button>
                <button
                  className="icon-btn ocr-block-copy-btn"
                  onClick={() => void copyText(result?.ocrText || "")}
                  aria-label="Copy OCR text"
                >
                  <Copy size={14} />
                </button>
              </div>
            </header>
            <section className="ocr-result-body-wrap">
              <pre>{result?.ocrText?.trim() || "等待识别..."}</pre>
            </section>
          </article>

          <article className="ocr-result-block">
            <header>
              <span>Output</span>
              <div className="ocr-block-actions">
                <button
                  className={`icon-btn ocr-block-favorite-btn${outputFavorited ? " active" : ""}`}
                  onClick={() => void addFavoriteText(outputFavoriteText, "output")}
                  aria-label="Favorite processed output"
                  title="收藏"
                  disabled={!outputFavoriteText}
                >
                  <Star size={14} />
                </button>
                <button
                  className={`icon-btn ocr-block-tts-btn${
                    ttsPlayingPanel === "output" ? " active" : ""
                  }`}
                  onClick={() => void speakPanel("output", outputSpeakableText)}
                  aria-label={ttsPlayingPanel === "output" ? "Stop speech" : "Play output text"}
                  title={ttsPlayingPanel === "output" ? "停止语音" : "语音播放"}
                  disabled={!outputSpeakableText || ttsLoadingPanel === "output"}
                >
                  <Volume2 size={14} />
                </button>
                <button
                  className="icon-btn ocr-block-copy-btn"
                  onClick={() => void copyText(outputText)}
                  aria-label="Copy processed output"
                >
                  <Copy size={14} />
                </button>
              </div>
            </header>
            <section className="ocr-result-body-wrap">
              <pre>{outputText}</pre>
            </section>
          </article>
        </section>
      </section>
    </main>
  );
}

function SettingsWindow({ settingsApi }: { settingsApi: SettingsApi }) {
  const {
    settings,
    loading,
    updating,
    error,
    updateSettings,
    setToggleShortcut,
    setToggleOcrShortcut,
    resetSettings,
    exportSettings,
    importSettings,
    refresh
  } = settingsApi;

  const [activeGroup, setActiveGroup] = useState<SettingGroup["key"]>("general");
  const [, setStatus] = useState("设置将自动保存");
  const [recordingMainShortcut, setRecordingMainShortcut] = useState(false);
  const [recordingOcrShortcut, setRecordingOcrShortcut] = useState(false);
  const [importText, setImportText] = useState("");
  const [exportText, setExportText] = useState("");
  const [pickingStorageFolder, setPickingStorageFolder] = useState(false);
  const [selectionBarDraftOrder, setSelectionBarDraftOrder] = useState<SelectionBarItemConfig[]>([]);
  const [agentModalOpen, setAgentModalOpen] = useState(false);
  const [agentDrafts, setAgentDrafts] = useState<CustomAgent[]>([]);
  const [activeAgentId, setActiveAgentId] = useState<string | null>(null);
  const [agentEditorError, setAgentEditorError] = useState<string | null>(null);
  const [runningApps, setRunningApps] = useState<string[]>([]);
  const [runningAppsLoading, setRunningAppsLoading] = useState(false);
  const [runningAppsFilter, setRunningAppsFilter] = useState("");
  const [testingLlmApi, setTestingLlmApi] = useState(false);
  const [testingOcrApi, setTestingOcrApi] = useState(false);
  const [llmApiTestFeedback, setLlmApiTestFeedback] = useState("");
  const [ocrApiTestFeedback, setOcrApiTestFeedback] = useState("");
  const [numberDrafts, setNumberDrafts] = useState<
    Partial<
      Record<
        | "selection-auto-hide"
        | "selection-min-chars"
        | "selection-max-chars"
        | "llm-temperature"
        | "llm-max-tokens"
        | "llm-timeout-ms"
        | "ocr-vision-temperature"
        | "ocr-vision-max-tokens"
        | "ocr-vision-timeout-ms"
        | "poll-ms"
        | "history-max"
        | "tts-rate",
        string
      >
    >
  >({});
  const [appVersion, setAppVersion] = useState("1.0.0");
  const [availableUpdateVersion, setAvailableUpdateVersion] = useState<string | null>(null);
  const [availableUpdateNotes, setAvailableUpdateNotes] = useState("");
  const [updateStatusText, setUpdateStatusText] = useState("尚未检查更新");
  const [updateChecking, setUpdateChecking] = useState(false);
  const [updateInstalling, setUpdateInstalling] = useState(false);
  const [updateDownloadProgress, setUpdateDownloadProgress] = useState<number | null>(null);
  const updateCheckBusyRef = useRef(false);
  const updateInstallBusyRef = useRef(false);
  const lastAutoUpdateCheckAtRef = useRef(0);
  const autoUpdateEntryArmedRef = useRef(true);
  const notifiedUpdateVersionInEntryRef = useRef<string | null>(null);

  useEffect(() => {
    if (error) {
      setStatus(error);
    }
  }, [error]);

  useEffect(() => {
    let active = true;
    void getVersion()
      .then((version) => {
        if (active) {
          setAppVersion(version || "1.0.0");
        }
      })
      .catch(() => {
        if (active) {
          setAppVersion("1.0.0");
        }
      });
    return () => {
      active = false;
    };
  }, []);

  async function checkForAppUpdates(options?: { silent?: boolean; notifyOnAvailable?: boolean }) {
    if (updateCheckBusyRef.current || updateInstallBusyRef.current) return;
    updateCheckBusyRef.current = true;
    const silent = Boolean(options?.silent);
    const notifyOnAvailable = Boolean(options?.notifyOnAvailable);
    setUpdateChecking(true);
    setUpdateDownloadProgress(null);
    try {
      if (!silent) {
        setUpdateStatusText("正在检查更新...");
      }
      const update = await check();
      if (!update) {
        setAvailableUpdateVersion(null);
        setAvailableUpdateNotes("");
        setUpdateStatusText("当前已经是最新版本");
        return;
      }

      setAvailableUpdateVersion(update.version || null);
      setAvailableUpdateNotes((update.body || "").trim());
      setUpdateStatusText(`发现新版本 v${update.version}，可直接下载并安装`);
      if (notifyOnAvailable) {
        const updateVersion = (update.version || "latest").trim() || "latest";
        if (notifiedUpdateVersionInEntryRef.current !== updateVersion) {
          notifiedUpdateVersionInEntryRef.current = updateVersion;
          setActiveGroup("about");
          const message =
            settings.language === "en-US"
              ? `A new version (v${updateVersion}) is available. You can update it in About & Diagnostics.`
              : `检测到新版本 v${updateVersion}，可在“关于与诊断”中下载并安装。`;
          window.alert(message);
        }
      }
    } catch (invokeError) {
      setUpdateStatusText(`检查更新失败：${String(invokeError)}`);
    } finally {
      updateCheckBusyRef.current = false;
      setUpdateChecking(false);
    }
  }

  async function downloadAndInstallAppUpdate() {
    if (updateCheckBusyRef.current || updateInstallBusyRef.current) return;
    updateInstallBusyRef.current = true;
    setUpdateInstalling(true);
    setUpdateDownloadProgress(0);
    try {
      const update = await check();
      if (!update) {
        setAvailableUpdateVersion(null);
        setAvailableUpdateNotes("");
        setUpdateStatusText("未发现可安装的新版本");
        setUpdateDownloadProgress(null);
        return;
      }

      let totalBytes = 0;
      let downloadedBytes = 0;
      setUpdateStatusText(`开始下载 v${update.version}...`);

      await update.downloadAndInstall((event) => {
        if (event.event === "Started") {
          totalBytes = Number(event.data.contentLength ?? 0);
          downloadedBytes = 0;
          setUpdateDownloadProgress(0);
          if (totalBytes > 0) {
            setUpdateStatusText(
              `正在下载更新包（${formatBytes(totalBytes)}）...`
            );
          } else {
            setUpdateStatusText("正在下载更新包...");
          }
          return;
        }

        if (event.event === "Progress") {
          downloadedBytes += Number(event.data.chunkLength ?? 0);
          if (totalBytes > 0) {
            const progress = Math.min(
              100,
              Math.max(0, Math.round((downloadedBytes / totalBytes) * 100))
            );
            setUpdateDownloadProgress(progress);
            setUpdateStatusText(
              `下载中 ${progress}%（${formatBytes(downloadedBytes)} / ${formatBytes(totalBytes)}）`
            );
          } else {
            setUpdateStatusText(`下载中（${formatBytes(downloadedBytes)}）`);
          }
          return;
        }

        if (event.event === "Finished") {
          setUpdateDownloadProgress(100);
          setUpdateStatusText("下载完成，正在安装...");
        }
      });

      setUpdateStatusText("安装完成，应用即将重启...");
      await relaunch();
    } catch (invokeError) {
      setUpdateStatusText(`安装更新失败：${String(invokeError)}`);
    } finally {
      updateInstallBusyRef.current = false;
      setUpdateInstalling(false);
    }
  }

  useEffect(() => {
    if (!recordingMainShortcut) return;

    const onKeyDown = (event: KeyboardEvent) => {
      event.preventDefault();
      event.stopPropagation();

      if (event.key === "Escape") {
        setRecordingMainShortcut(false);
        setStatus("已取消快捷键录制");
        return;
      }

      const shortcut = buildShortcutFromEvent(event);
      if (!shortcut) {
        setStatus("快捷键需包含至少一个修饰键（Ctrl/Alt/Shift/Meta）");
        return;
      }

      setRecordingMainShortcut(false);
      void (async () => {
        const result = await setToggleShortcut(shortcut);
        if (result) {
          setStatus(`快捷键已更新为 ${shortcut}`);
        }
      })();
    };

    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, [recordingMainShortcut, setToggleShortcut]);

  useEffect(() => {
    if (!recordingOcrShortcut) return;

    const onKeyDown = (event: KeyboardEvent) => {
      event.preventDefault();
      event.stopPropagation();

      if (event.key === "Escape") {
        setRecordingOcrShortcut(false);
        setStatus("已取消 OCR 快捷键录制");
        return;
      }

      const shortcut = buildShortcutFromEvent(event);
      if (!shortcut) {
        setStatus("快捷键需包含至少一个修饰键（Ctrl/Alt/Shift/Meta）");
        return;
      }

      setRecordingOcrShortcut(false);
      void (async () => {
        const result = await setToggleOcrShortcut(shortcut);
        if (result) {
          setStatus(`OCR 快捷键已更新为 ${shortcut}`);
        }
      })();
    };

    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, [recordingOcrShortcut, setToggleOcrShortcut]);

  useEffect(() => {
    if (activeGroup !== "selectionAssistant") return;
    if (runningAppsLoading || runningApps.length > 0) return;
    void refreshRunningApps();
  }, [activeGroup, runningApps.length, runningAppsLoading]);

  useEffect(() => {
    if (!settings.window.checkUpdatesOnStartup) {
      autoUpdateEntryArmedRef.current = true;
      notifiedUpdateVersionInEntryRef.current = null;
      return;
    }

    const triggerCheckForCurrentEntry = () => {
      if (!autoUpdateEntryArmedRef.current) return;
      autoUpdateEntryArmedRef.current = false;
      void (async () => {
        let visible = true;
        let focused = true;
        try {
          const win = getCurrentWebviewWindow();
          visible = await win.isVisible();
          focused = await win.isFocused();
        } catch {
          focused = document.hasFocus();
        }
        if (!visible || !focused) {
          autoUpdateEntryArmedRef.current = true;
          return;
        }

        const now = Date.now();
        if (now - lastAutoUpdateCheckAtRef.current < 1500) {
          autoUpdateEntryArmedRef.current = true;
          return;
        }
        lastAutoUpdateCheckAtRef.current = now;
        await checkForAppUpdates({ silent: true, notifyOnAvailable: true });
      })();
    };

    const triggerCheckOnSettingsEntry = () => {
      autoUpdateEntryArmedRef.current = true;
      notifiedUpdateVersionInEntryRef.current = null;
      triggerCheckForCurrentEntry();
    };

    const onVisibility = () => {
      if (document.hidden) {
        autoUpdateEntryArmedRef.current = true;
        notifiedUpdateVersionInEntryRef.current = null;
        return;
      }
      triggerCheckForCurrentEntry();
    };

    let unlistenSettingsShown: (() => void) | null = null;
    let visibilityPollTimer: number | null = null;
    let lastKnownWindowVisible = true;
    void getCurrentWebviewWindow()
      .listen<boolean>("snapparse://settings-window-shown", () => {
        triggerCheckOnSettingsEntry();
      })
      .then((off) => {
        unlistenSettingsShown = off;
      });

    visibilityPollTimer = window.setInterval(() => {
      void getCurrentWebviewWindow()
        .isVisible()
        .then((visible) => {
          if (!visible && lastKnownWindowVisible) {
            autoUpdateEntryArmedRef.current = true;
            notifiedUpdateVersionInEntryRef.current = null;
          }
          if (visible && !lastKnownWindowVisible) {
            triggerCheckOnSettingsEntry();
          }
          lastKnownWindowVisible = visible;
        })
        .catch(() => {
          // ignore visibility polling failures
        });
    }, 900);

    triggerCheckOnSettingsEntry();
    document.addEventListener("visibilitychange", onVisibility);
    return () => {
      if (unlistenSettingsShown) {
        unlistenSettingsShown();
      }
      if (visibilityPollTimer !== null) {
        window.clearInterval(visibilityPollTimer);
      }
      document.removeEventListener("visibilitychange", onVisibility);
    };
  }, [settings.window.checkUpdatesOnStartup]);

  async function applyPatch(patch: AppSettingsPatch, successMessage?: string) {
    const result = await updateSettings(patch);
    if (result && successMessage) {
      setStatus(successMessage);
    }
  }

  function normalizeBlockedApps(values: string[]) {
    const unique = new Set<string>();
    const normalized: string[] = [];
    for (const item of values) {
      const value = item.trim().toLowerCase();
      if (!value || unique.has(value)) continue;
      unique.add(value);
      normalized.push(value);
    }
    return normalized;
  }

  const blockedAppSet = useMemo(() => {
    return new Set(settings.selectionAssistant.blockedApps.map((item) => item.toLowerCase()));
  }, [settings.selectionAssistant.blockedApps]);

  const filteredRunningApps = useMemo(() => {
    const query = runningAppsFilter.trim().toLowerCase();
    const base = runningApps.filter((item) =>
      query ? item.toLowerCase().includes(query) : true
    );
    return base.slice(0, 400);
  }, [runningApps, runningAppsFilter]);

  async function refreshRunningApps() {
    if (runningAppsLoading) return;
    setRunningAppsLoading(true);
    try {
      const payload = await invoke<string[]>("list_running_apps_cmd");
      const unique = new Set<string>();
      const normalized = (Array.isArray(payload) ? payload : [])
        .map((item) => item.trim())
        .filter((item) => {
          const key = item.toLowerCase();
          if (!item || unique.has(key)) return false;
          unique.add(key);
          return true;
        })
        .sort((a, b) => a.toLowerCase().localeCompare(b.toLowerCase()));
      setRunningApps(normalized);
      setStatus(`已加载 ${normalized.length} 个运行中应用`);
    } catch (invokeError) {
      setStatus(String(invokeError));
    } finally {
      setRunningAppsLoading(false);
    }
  }

  async function testLlmApiConfig() {
    if (testingLlmApi) return;
    setTestingLlmApi(true);
    setLlmApiTestFeedback("");
    try {
      const summary = await invoke<string>("test_llm_api_cmd");
      const nextFeedback = summary ? `测试通过：${summary}` : "测试通过";
      setStatus(`大模型 API ${nextFeedback}`);
      setLlmApiTestFeedback(nextFeedback);
    } catch (invokeError) {
      const nextFeedback = String(invokeError);
      setStatus(nextFeedback);
      setLlmApiTestFeedback(nextFeedback);
    } finally {
      setTestingLlmApi(false);
    }
  }

  async function testOcrApiConfig() {
    if (testingOcrApi) return;
    setTestingOcrApi(true);
    setOcrApiTestFeedback("");
    try {
      const summary = await invoke<string>("test_ocr_vision_api_cmd");
      const nextFeedback = summary ? `测试通过：${summary}` : "测试通过";
      setStatus(`OCR 模型 API ${nextFeedback}`);
      setOcrApiTestFeedback(nextFeedback);
    } catch (invokeError) {
      const nextFeedback = String(invokeError);
      setStatus(nextFeedback);
      setOcrApiTestFeedback(nextFeedback);
    } finally {
      setTestingOcrApi(false);
    }
  }

  function toggleBlockedApp(name: string, enabled: boolean) {
    const key = name.trim().toLowerCase();
    if (!key) return;
    const current = normalizeBlockedApps(settings.selectionAssistant.blockedApps);
    const currentSet = new Set(current);
    if (enabled) {
      if (currentSet.has(key)) return;
      const next = [...current, key];
      void applyPatch(
        {
          selectionAssistant: {
            blockedApps: next
          }
        },
        "忽略应用名单已更新"
      );
      return;
    }
    if (!currentSet.has(key)) return;
    const next = current.filter((item) => item !== key);
    void applyPatch(
      {
        selectionAssistant: {
          blockedApps: next
        }
      },
      "忽略应用名单已更新"
    );
  }

  function getNumberInputValue(
    key:
      | "selection-auto-hide"
      | "selection-min-chars"
      | "selection-max-chars"
      | "llm-temperature"
      | "llm-max-tokens"
      | "llm-timeout-ms"
      | "ocr-vision-temperature"
      | "ocr-vision-max-tokens"
      | "ocr-vision-timeout-ms"
      | "poll-ms"
      | "history-max"
      | "tts-rate",
    persisted: number
  ) {
    return numberDrafts[key] ?? String(persisted);
  }

  function setNumberInputValue(
    key:
      | "selection-auto-hide"
      | "selection-min-chars"
      | "selection-max-chars"
      | "llm-temperature"
      | "llm-max-tokens"
      | "llm-timeout-ms"
      | "ocr-vision-temperature"
      | "ocr-vision-max-tokens"
      | "ocr-vision-timeout-ms"
      | "poll-ms"
      | "history-max"
      | "tts-rate",
    value: string
  ) {
    setNumberDrafts((prev) => ({
      ...prev,
      [key]: value
    }));
  }

  function clearNumberInputDraft(
    key:
      | "selection-auto-hide"
      | "selection-min-chars"
      | "selection-max-chars"
      | "llm-temperature"
      | "llm-max-tokens"
      | "llm-timeout-ms"
      | "ocr-vision-temperature"
      | "ocr-vision-max-tokens"
      | "ocr-vision-timeout-ms"
      | "poll-ms"
      | "history-max"
      | "tts-rate"
  ) {
    setNumberDrafts((prev) => {
      if (!(key in prev)) return prev;
      const next = { ...prev };
      delete next[key];
      return next;
    });
  }

  function parseNumberInput(raw: string) {
    const trimmed = raw.trim();
    if (!trimmed) return null;
    const parsed = Number(trimmed);
    if (!Number.isFinite(parsed)) return null;
    return parsed;
  }

  function clampNumber(value: number, min: number, max: number) {
    return Math.min(max, Math.max(min, value));
  }

  function blurNumberInputOnEnter(event: ReactKeyboardEvent<HTMLInputElement>) {
    if (event.key === "Enter") {
      event.preventDefault();
      event.currentTarget.blur();
    }
  }

  async function commitIntegerInput(
    key:
      | "selection-auto-hide"
      | "selection-min-chars"
      | "selection-max-chars"
      | "llm-max-tokens"
      | "llm-timeout-ms"
      | "ocr-vision-max-tokens"
      | "ocr-vision-timeout-ms"
      | "poll-ms"
      | "history-max"
      | "tts-rate",
    persisted: number,
    min: number,
    max: number,
    patchFactory: (next: number) => AppSettingsPatch
  ) {
    const raw = numberDrafts[key];
    if (raw === undefined) return;
    clearNumberInputDraft(key);

    const parsed = parseNumberInput(raw);
    if (parsed === null) return;

    const next = Math.round(clampNumber(parsed, min, max));
    if (next === persisted) return;
    await applyPatch(patchFactory(next));
  }

  async function commitFloatInput(
    key: "llm-temperature" | "ocr-vision-temperature",
    persisted: number,
    min: number,
    max: number,
    precision: number,
    patchFactory: (next: number) => AppSettingsPatch
  ) {
    const raw = numberDrafts[key];
    if (raw === undefined) return;
    clearNumberInputDraft(key);

    const parsed = parseNumberInput(raw);
    if (parsed === null) return;

    const clamped = clampNumber(parsed, min, max);
    const next = Number(clamped.toFixed(precision));
    if (Math.abs(next - persisted) < 0.0001) return;
    await applyPatch(patchFactory(next));
  }

  async function pickHistoryStorageFolder() {
    if (pickingStorageFolder) return;
    setPickingStorageFolder(true);
    try {
      const selected = await invoke<string | null>("pick_history_storage_folder");
      if (!selected) {
        setStatus("已取消选择文件夹");
        return;
      }
      await applyPatch(
        {
          history: { storagePath: selected }
        },
        "复制内容存储位置已更新"
      );
    } catch (invokeError) {
      setStatus(String(invokeError));
    } finally {
      setPickingStorageFolder(false);
    }
  }

  async function clearHistory() {
    try {
      await invoke("clear_history");
      setStatus("历史记录已清理");
    } catch (invokeError) {
      setStatus(String(invokeError));
    }
  }

  async function exportSettingsToTextarea() {
    const payload = await exportSettings();
    if (payload) {
      setExportText(payload);
      setStatus("配置已导出到下方文本区域");
    }
  }

  async function importSettingsFromTextarea() {
    const trimmed = importText.trim();
    if (!trimmed) {
      setStatus("请先粘贴要导入的 JSON 配置");
      return;
    }

    const result = await importSettings(trimmed);
    if (result) {
      setStatus("配置导入成功");
      setImportText("");
    }
  }

  async function copyExportJson() {
    if (!exportText.trim()) {
      setStatus("请先点击“导出配置”生成内容");
      return;
    }

    try {
      await navigator.clipboard.writeText(exportText);
      setStatus("导出 JSON 已复制到剪贴板");
    } catch (copyError) {
      setStatus(String(copyError));
    }
  }

  async function resetAllSettings() {
    const result = await resetSettings();
    if (result) {
      setStatus("已恢复默认设置");
    }
  }

  const orderedSelectionBarItems = useMemo(
    () => normalizeSelectionBarOrder(settings.agents.barOrder, settings.agents.custom),
    [settings.agents.barOrder, settings.agents.custom]
  );

  useEffect(() => {
    setSelectionBarDraftOrder(orderedSelectionBarItems);
  }, [orderedSelectionBarItems]);

  async function saveSelectionBarOrder(
    nextOrder: SelectionBarItemConfig[],
    successMessage = "条形栏排序已更新"
  ) {
    const normalized = normalizeSelectionBarOrder(nextOrder, settings.agents.custom);
    setSelectionBarDraftOrder(normalized);
    await applyPatch(
      {
        agents: {
          barOrder: normalized
        }
      },
      successMessage
    );
  }

  function setSelectionBarItemEnabled(key: SelectionBarActionKey, enabled: boolean) {
    if (enabled) {
      const current = selectionBarDraftOrder.find((item) => item.key === key);
      const enabledCount = selectionBarDraftOrder.filter((item) => item.enabled).length;
      if (!current?.enabled && enabledCount >= MAX_SELECTION_BAR_ENABLED_ITEMS) {
        const ordered = [...selectionBarDraftOrder].sort((a, b) => a.order - b.order);
        const victim = [...ordered]
          .reverse()
          .find((item) => item.enabled && item.key !== key);
        if (!victim) {
          setStatus(`条形栏最多启用 ${MAX_SELECTION_BAR_ENABLED_ITEMS} 个功能`);
          return;
        }
        const swapped = ordered.map((item) => {
          if (item.key === key) {
            return {
              ...item,
              enabled: true
            };
          }
          if (item.key === victim.key) {
            return {
              ...item,
              enabled: false
            };
          }
          return item;
        });
        void saveSelectionBarOrder(swapped, "已达上限，自动替换一个功能");
        return;
      }
    }
    const next = selectionBarDraftOrder.map((item) =>
      item.key === key
        ? {
            ...item,
            enabled
          }
        : item
    );
    void saveSelectionBarOrder(next, "条形栏显示项已更新");
  }

  function reorderSelectionBarItems(fromKey: string, toKey: string) {
    if (!fromKey || !toKey || fromKey === toKey) return;
    const ordered = [...selectionBarDraftOrder].sort((a, b) => a.order - b.order);
    const fromIndex = ordered.findIndex((item) => item.key === fromKey);
    const toIndex = ordered.findIndex((item) => item.key === toKey);
    if (fromIndex < 0 || toIndex < 0) return;
    const [moved] = ordered.splice(fromIndex, 1);
    ordered.splice(toIndex, 0, moved);
    const next = ordered.map((item, index) => ({
      ...item,
      order: index
    }));
    void saveSelectionBarOrder(next);
  }

  function moveSelectionBarItem(key: string, delta: -1 | 1) {
    const ordered = [...selectionBarDraftOrder].sort((a, b) => a.order - b.order);
    const index = ordered.findIndex((item) => item.key === key);
    if (index < 0) return;
    const targetIndex = index + delta;
    if (targetIndex < 0 || targetIndex >= ordered.length) return;
    const targetKey = ordered[targetIndex].key;
    reorderSelectionBarItems(key, targetKey);
  }

  async function deleteCustomAgentFromOrder(customId: string) {
    const agent = settings.agents.custom.find((item) => item.id === customId);
    if (!agent) return;
    const displayName = agent.name.trim() || "该 Agent";
    if (!window.confirm(`确认删除 ${displayName}？`)) return;

    const nextCustom = normalizeCustomAgents(
      settings.agents.custom.filter((item) => item.id !== customId)
    );
    const nextOrder = normalizeSelectionBarOrder(
      selectionBarDraftOrder.filter((item) => item.key !== (`custom:${customId}` as SelectionBarActionKey)),
      nextCustom
    );
    setSelectionBarDraftOrder(nextOrder);
    if (agentModalOpen && activeAgentId === customId) {
      closeAgentManager();
    }
    await applyPatch(
      {
        agents: {
          custom: nextCustom,
          barOrder: nextOrder
        }
      },
      "已删除自定义 Agent"
    );
  }

  function openAgentManager(options?: { agentId?: string; create?: boolean }) {
    let custom = normalizeCustomAgents(settings.agents.custom);
    let targetId: string | null = null;

    if (options?.create) {
      if (custom.length >= CUSTOM_AGENT_MAX_COUNT) {
        setStatus(`最多可创建 ${CUSTOM_AGENT_MAX_COUNT} 个自定义 Agent`);
        return;
      }
      const nextAgent = makeDefaultCustomAgent(custom.length);
      custom = [...custom, nextAgent];
      targetId = nextAgent.id;
    }

    if (!targetId && options?.agentId && custom.some((item) => item.id === options.agentId)) {
      targetId = options.agentId;
    }

    if (!targetId && custom.length === 0) {
      const nextAgent = makeDefaultCustomAgent(0);
      custom = [nextAgent];
      targetId = nextAgent.id;
    }

    if (!targetId) {
      targetId = custom[0]?.id ?? null;
    }

    setAgentDrafts(custom);
    setActiveAgentId(targetId);
    setAgentEditorError(null);
    setAgentModalOpen(true);
  }

  function closeAgentManager() {
    setAgentModalOpen(false);
    setAgentEditorError(null);
  }

  function setCustomAgentDraft(id: string, patch: Partial<CustomAgent>) {
    setAgentDrafts((prev) =>
      prev.map((agent) => {
        if (agent.id !== id) return agent;
        const merged = {
          ...agent,
          ...patch
        };
        if (patch.name !== undefined) {
          merged.name = trimNameByUnits(patch.name);
        }
        return merged;
      })
    );
  }

  async function saveAgentManager() {
    const normalizedCustom = normalizeCustomAgents(agentDrafts);
    for (const item of normalizedCustom) {
      const errorMessage = validateAgentName(item.name);
      if (errorMessage) {
        setAgentEditorError(`「${item.name || "未命名 Agent"}」: ${errorMessage}`);
        return;
      }
    }

    const normalizedBarOrder = normalizeSelectionBarOrder(
      selectionBarDraftOrder,
      normalizedCustom
    );
    await applyPatch(
      {
        agents: {
          custom: normalizedCustom,
          barOrder: normalizedBarOrder
        }
      },
      "自定义 Agent 与条形栏排序已更新"
    );
    closeAgentManager();
  }

  const activeAgentDraft = useMemo(
    () => agentDrafts.find((item) => item.id === activeAgentId) ?? null,
    [activeAgentId, agentDrafts]
  );

  const ocrCustomActionOptions = useMemo(
    () =>
      settings.agents.custom
        .slice()
        .sort((a, b) => a.order - b.order)
        .map((agent) => ({
          value: `${OCR_CUSTOM_ACTION_PREFIX}${agent.id}`,
          label: `${iconGlyphForCustomAgent(agent.icon)} ${agent.name || "未命名 Agent"}`
        })),
    [settings.agents.custom]
  );

  const ocrActionSelectValue = useMemo(() => {
    if (settings.ocr.defaultAction !== "custom") {
      return settings.ocr.defaultAction;
    }
    const matched = ocrCustomActionOptions.some(
      (item) => item.value === `${OCR_CUSTOM_ACTION_PREFIX}${settings.ocr.customAgentId}`
    );
    if (matched && settings.ocr.customAgentId) {
      return `${OCR_CUSTOM_ACTION_PREFIX}${settings.ocr.customAgentId}`;
    }
    return "custom";
  }, [ocrCustomActionOptions, settings.ocr.customAgentId, settings.ocr.defaultAction]);

  function applyOcrDefaultActionFromSelect(value: string) {
    if (value.startsWith(OCR_CUSTOM_ACTION_PREFIX)) {
      const customAgentId = value.slice(OCR_CUSTOM_ACTION_PREFIX.length).trim();
      if (!customAgentId) {
        void applyPatch({
          ocr: {
            defaultAction: "custom"
          }
        });
        return;
      }
      void applyPatch({
        ocr: {
          defaultAction: "custom",
          customAgentId
        }
      });
      return;
    }

    void applyPatch({
      ocr: {
        defaultAction: value as OcrActionKind
      }
    });
  }

  const currentGroup = SETTING_GROUPS.find((group) => group.key === activeGroup) ?? SETTING_GROUPS[0];

  if (loading) {
    return (
      <main className="window-root settings-window">
        <aside className="settings-sidebar" aria-label="Settings categories">
          <nav className="sidebar-nav">
            {SETTING_GROUPS.map((group) => {
              const Icon = group.icon;
              return (
                <button
                  key={group.key}
                  className={`sidebar-item${activeGroup === group.key ? " active" : ""}`}
                  onClick={() => setActiveGroup(group.key)}
                >
                  <Icon size={16} />
                  <span>{group.label}</span>
                </button>
              );
            })}
          </nav>
        </aside>
        <section className="settings-content">
          <header className="settings-head" data-tauri-drag-region>
            <div data-tauri-drag-region>
              <h1>{currentGroup.label}</h1>
              <p>{currentGroup.description}</p>
            </div>
          </header>
          <section className="settings-stack">
            <article className="settings-card">
              <h2>正在加载配置...</h2>
              <p className="help-text">正在读取本地设置与历史数据，请稍候。</p>
            </article>
          </section>
        </section>
      </main>
    );
  }

  return (
    <main className="window-root settings-window">
      <aside className="settings-sidebar" aria-label="Settings categories">
        <nav className="sidebar-nav">
          {SETTING_GROUPS.map((group) => {
            const Icon = group.icon;
            return (
              <button
                key={group.key}
                className={`sidebar-item${activeGroup === group.key ? " active" : ""}`}
                onClick={() => setActiveGroup(group.key)}
              >
                <Icon size={16} />
                <span>{group.label}</span>
              </button>
            );
          })}
        </nav>

      </aside>

      <section className="settings-content">
        <header className="settings-head" data-tauri-drag-region>
          <div data-tauri-drag-region>
            <h1>{currentGroup.label}</h1>
            <p>{currentGroup.description}</p>
          </div>
        </header>

        {activeGroup === "general" && (
          <section className="settings-stack">
            <article className="settings-card">
              <h2>主题预设</h2>
              <div className="filled-control">
                <label htmlFor="theme-preset">主题</label>
                <select
                  id="theme-preset"
                  className="md2-select"
                  aria-label="主题预设"
                  disabled={updating}
                  value={settings.themePreset}
                  onChange={(event) => {
                    void applyPatch({ themePreset: parseThemePreset(event.target.value) });
                  }}
                >
                  {THEME_OPTIONS.map((item) => (
                    <option key={item.key} value={item.key}>
                      {item.label}
                    </option>
                  ))}
                </select>
              </div>
              <div className="filled-control">
                <label htmlFor="lang">语言</label>
                <select
                  id="lang"
                  className="md2-select"
                  aria-label="界面语言"
                  disabled={updating}
                  value={settings.language}
                  onChange={(event) => {
                    void applyPatch({ language: event.target.value === "en-US" ? "en-US" : "zh-CN" });
                  }}
                >
                  <option value="zh-CN">简体中文</option>
                  <option value="en-US">English</option>
                </select>
              </div>
            </article>

            <article className="settings-card">
              <h2>窗口自动行为</h2>
              <label className="check-row">
                <span>窗口失焦时自动关闭</span>
                <input
                  className="md2-check"
                  type="checkbox"
                  checked={settings.window.autoHideOnBlur}
                  onChange={(event) => {
                    void applyPatch({ window: { autoHideOnBlur: event.target.checked } });
                  }}
                />
              </label>
              <label className="check-row">
                <span>记忆窗口大小（剪贴板/OCR/划词处理窗口）</span>
                <input
                  className="md2-check"
                  type="checkbox"
                  checked={settings.window.rememberMainWindowSize}
                  onChange={(event) => {
                    void applyPatch({ window: { rememberMainWindowSize: event.target.checked } });
                  }}
                />
              </label>
              <label className="check-row">
                <span>记忆窗口位置（关闭后跟随鼠标/光标呼出）</span>
                <input
                  className="md2-check"
                  type="checkbox"
                  checked={settings.window.rememberPosition}
                  onChange={(event) => {
                    void applyPatch({ window: { rememberPosition: event.target.checked } });
                  }}
                />
              </label>
              <label className="check-row">
                <span>开机自动启动</span>
                <input
                  className="md2-check"
                  type="checkbox"
                  checked={settings.window.launchOnSystemStartup}
                  onChange={(event) => {
                    void applyPatch({ window: { launchOnSystemStartup: event.target.checked } });
                  }}
                />
              </label>
              <label className="check-row">
                <span>自动静默启动（仅开机自启时生效）</span>
                <input
                  className="md2-check"
                  type="checkbox"
                  checked={settings.window.silentStartup}
                  onChange={(event) => {
                    void applyPatch({ window: { silentStartup: event.target.checked } });
                  }}
                />
              </label>
              <label className="check-row">
                <span>启动时检查更新</span>
                <input
                  className="md2-check"
                  type="checkbox"
                  checked={settings.window.checkUpdatesOnStartup}
                  onChange={(event) => {
                    void applyPatch({ window: { checkUpdatesOnStartup: event.target.checked } });
                  }}
                />
              </label>
            </article>

          </section>
        )}
        {activeGroup === "selectionAssistant" && (
          <section className="settings-stack">
            <article className="settings-card">
              <div className="settings-card-head-inline">
                <h2>条形栏功能排序</h2>
                <button
                  className="tonal-btn compact"
                  onClick={() => openAgentManager({ create: true })}
                >
                  <Bot size={13} />
                  <span>自定义 Agent</span>
                </button>
              </div>
              <div className="bar-order-list settings-order-list">
                {selectionBarDraftOrder.map((item, index) => {
                  const customId = parseCustomAgentActionKey(item.key);
                  const builtin = isBuiltinSelectionBarActionKey(item.key)
                    ? BUILTIN_SELECTION_BAR_ACTION_MAP[item.key]
                    : null;
                  const agent = customId
                    ? settings.agents.custom.find((entry) => entry.id === customId) ?? null
                    : null;
                  if (!builtin && !agent) return null;

                  const Icon = builtin ? builtin.icon : iconForCustomAgent(agent?.icon ?? "Bot");
                  const label = builtin ? builtin.label : agent?.name || "未命名 Agent";
                  const isFirst = index === 0;
                  const isLast = index === selectionBarDraftOrder.length - 1;
                  const canEditCustom = Boolean(customId && agent);

                  return (
                    <div
                      key={item.key}
                      className="bar-order-item"
                    >
                      <Icon size={13} />
                      <span className="bar-order-label">{label}</span>
                      <div className="bar-order-actions">
                        {canEditCustom && (
                          <>
                            <button
                              className="icon-btn compact"
                              onClick={() => openAgentManager({ agentId: agent!.id })}
                              aria-label="编辑 Agent"
                              title="编辑 Agent"
                            >
                              <PenTool size={12} />
                            </button>
                            <button
                              className="icon-btn compact danger"
                              onClick={() => void deleteCustomAgentFromOrder(agent!.id)}
                              aria-label="删除 Agent"
                              title="删除 Agent"
                            >
                              <Trash2 size={12} />
                            </button>
                          </>
                        )}
                        <button
                          className="icon-btn compact"
                          onClick={() => moveSelectionBarItem(item.key, -1)}
                          disabled={isFirst}
                          aria-label="上移"
                          title="上移"
                        >
                          <ChevronUp size={13} />
                        </button>
                        <button
                          className="icon-btn compact"
                          onClick={() => moveSelectionBarItem(item.key, 1)}
                          disabled={isLast}
                          aria-label="下移"
                          title="下移"
                        >
                          <ChevronDown size={13} />
                        </button>
                        <input
                          className="md2-check"
                          type="checkbox"
                          checked={item.enabled}
                          onChange={(event) => {
                            setSelectionBarItemEnabled(item.key, event.target.checked);
                          }}
                        />
                      </div>
                    </div>
                  );
                })}
              </div>
            </article>

            <article className="settings-card">
              <h2>划词助手行为</h2>
              <label className="check-row">
                <span>启用划词助手</span>
                <input
                  className="md2-check"
                  type="checkbox"
                  checked={settings.selectionAssistant.enabled}
                  onChange={(event) => {
                    void applyPatch({
                      selectionAssistant: { enabled: event.target.checked }
                    });
                  }}
                />
              </label>

              <div className="filled-control">
                <label htmlFor="selection-mode">触发模式</label>
                <select
                  id="selection-mode"
                  className="md2-select"
                  value={settings.selectionAssistant.mode}
                  onChange={(event) => {
                    void applyPatch({
                      selectionAssistant: {
                        mode: parseSelectionMode(event.target.value)
                      }
                    });
                  }}
                >
                  {SELECTION_MODE_OPTIONS.map((item) => (
                    <option key={item.key} value={item.key}>
                      {item.label}
                    </option>
                  ))}
                </select>
              </div>

              <div className="filled-control">
                <label htmlFor="selection-default-translate-to">默认翻译目标语言</label>
                <select
                  id="selection-default-translate-to"
                  className="md2-select"
                  value={settings.selectionAssistant.defaultTranslateTo}
                  onChange={(event) => {
                    void applyPatch({
                      selectionAssistant: {
                        defaultTranslateTo: parseTranslateTarget(event.target.value)
                      }
                    });
                  }}
                >
                  {TRANSLATE_LANGUAGE_OPTIONS.filter((item) => item.key !== "auto").map((item) => (
                    <option key={`selection-default-target-${item.key}`} value={item.key}>
                      {item.label}
                    </option>
                  ))}
                </select>
              </div>

              <div className="filled-control">
                <label htmlFor="selection-search-url">搜索 URL 模板</label>
                <input
                  id="selection-search-url"
                  value={settings.selectionAssistant.searchUrlTemplate}
                  onChange={(event) => {
                    void applyPatch({
                      selectionAssistant: { searchUrlTemplate: event.target.value }
                    });
                  }}
                />
              </div>

              <div className="filled-control">
                <label htmlFor="selection-auto-hide">条形栏自动隐藏 (ms)</label>
                <input
                  id="selection-auto-hide"
                  type="number"
                  min={SELECTION_AUTO_HIDE_RANGE.min}
                  max={SELECTION_AUTO_HIDE_RANGE.max}
                  value={getNumberInputValue(
                    "selection-auto-hide",
                    settings.selectionAssistant.autoHideMs
                  )}
                  onChange={(event) => {
                    setNumberInputValue("selection-auto-hide", event.target.value);
                  }}
                  onKeyDown={blurNumberInputOnEnter}
                  onBlur={() => {
                    void commitIntegerInput(
                      "selection-auto-hide",
                      settings.selectionAssistant.autoHideMs,
                      SELECTION_AUTO_HIDE_RANGE.min,
                      SELECTION_AUTO_HIDE_RANGE.max,
                      (next) => ({
                        selectionAssistant: { autoHideMs: next }
                      })
                    );
                  }}
                />
              </div>
              <div className="filled-control">
                <label htmlFor="selection-min-chars">最小字符数</label>
                <input
                  id="selection-min-chars"
                  type="number"
                  min={SELECTION_MIN_CHARS_RANGE.min}
                  max={SELECTION_MIN_CHARS_RANGE.max}
                  value={getNumberInputValue(
                    "selection-min-chars",
                    settings.selectionAssistant.minChars
                  )}
                  onChange={(event) => {
                    setNumberInputValue("selection-min-chars", event.target.value);
                  }}
                  onKeyDown={blurNumberInputOnEnter}
                  onBlur={() => {
                    void commitIntegerInput(
                      "selection-min-chars",
                      settings.selectionAssistant.minChars,
                      SELECTION_MIN_CHARS_RANGE.min,
                      SELECTION_MIN_CHARS_RANGE.max,
                      (next) => ({
                        selectionAssistant: { minChars: next }
                      })
                    );
                  }}
                />
              </div>
              <div className="filled-control">
                <label htmlFor="selection-max-chars">最大字符数</label>
                <input
                  id="selection-max-chars"
                  type="number"
                  min={SELECTION_MAX_CHARS_RANGE.min}
                  max={SELECTION_MAX_CHARS_RANGE.max}
                  value={getNumberInputValue(
                    "selection-max-chars",
                    settings.selectionAssistant.maxChars
                  )}
                  onChange={(event) => {
                    setNumberInputValue("selection-max-chars", event.target.value);
                  }}
                  onKeyDown={blurNumberInputOnEnter}
                  onBlur={() => {
                    void commitIntegerInput(
                      "selection-max-chars",
                      settings.selectionAssistant.maxChars,
                      SELECTION_MAX_CHARS_RANGE.min,
                      SELECTION_MAX_CHARS_RANGE.max,
                      (next) => ({
                        selectionAssistant: { maxChars: next }
                      })
                    );
                  }}
                />
              </div>
              <div className="filled-control">
                <label htmlFor="selection-blocked-apps">忽略应用（每行一个进程名）</label>
                <textarea
                  id="selection-blocked-apps"
                  className="settings-json"
                  placeholder="请输入应用的执行文件名，每行一个，不区分大小写，可以模糊匹配。例如： chrome.exe、weixin.exe、Cherry Studio.exe等"
                  value={settings.selectionAssistant.blockedApps.join("\n")}
                  onChange={(event) => {
                    const blockedApps = normalizeBlockedApps(event.target.value.split(/\r?\n/));
                    void applyPatch({
                      selectionAssistant: { blockedApps }
                    });
                  }}
                />
              </div>
              <div className="filled-control">
                <label htmlFor="selection-blocked-app-filter">应用筛选名单（运行中）</label>
                <div className="blocked-app-picker-toolbar">
                  <input
                    id="selection-blocked-app-filter"
                    value={runningAppsFilter}
                    placeholder="搜索应用（如 chrome、weixin、powershell）"
                    onChange={(event) => setRunningAppsFilter(event.target.value)}
                  />
                  <button
                    type="button"
                    className="path-picker-btn"
                    onClick={() => void refreshRunningApps()}
                    disabled={runningAppsLoading}
                  >
                    <RefreshCw size={13} />
                    <span>{runningAppsLoading ? "刷新中..." : "刷新"}</span>
                  </button>
                </div>
                <div className="blocked-app-picker-list" aria-label="Running app filter list">
                  {filteredRunningApps.length === 0 ? (
                    <p className="help-text">
                      {runningAppsLoading
                        ? "正在获取运行中应用..."
                        : "暂无可用应用，请点击“刷新”获取当前运行中的应用。"}
                    </p>
                  ) : (
                    filteredRunningApps.map((name) => {
                      const key = name.toLowerCase();
                      return (
                        <label key={name} className="blocked-app-picker-item">
                          <span>{name}</span>
                          <input
                            className="md2-check"
                            type="checkbox"
                            checked={blockedAppSet.has(key)}
                            onChange={(event) => toggleBlockedApp(name, event.target.checked)}
                          />
                        </label>
                      );
                    })
                  )}
                </div>
              </div>

              <label className="check-row">
                <span>显示软件图标动画</span>
                <input
                  className="md2-check"
                  type="checkbox"
                  checked={settings.selectionAssistant.showIconAnimation}
                  onChange={(event) => {
                    void applyPatch({
                      selectionAssistant: { showIconAnimation: event.target.checked }
                    });
                  }}
                />
              </label>
              <label className="check-row">
                <span>紧凑模式（条形栏仅显示图标）</span>
                <input
                  className="md2-check"
                  type="checkbox"
                  checked={settings.selectionAssistant.compactMode}
                  onChange={(event) => {
                    void applyPatch({
                      selectionAssistant: { compactMode: event.target.checked }
                    });
                  }}
                />
              </label>
              <label className="check-row">
                <span>结果窗口默认置顶</span>
                <input
                  className="md2-check"
                  type="checkbox"
                  checked={settings.selectionAssistant.resultWindowAlwaysOnTop}
                  onChange={(event) => {
                    void applyPatch({
                      selectionAssistant: { resultWindowAlwaysOnTop: event.target.checked }
                    });
                  }}
                />
              </label>
            </article>

            <article className="settings-card">
              <h2>大模型 API（OpenAI-compatible）</h2>

              <div className="filled-control">
                <label htmlFor="llm-base-url">Base URL</label>
                <input
                  id="llm-base-url"
                  value={settings.llm.baseUrl}
                  onChange={(event) => {
                    void applyPatch({ llm: { baseUrl: event.target.value } });
                  }}
                />
              </div>
              <div className="filled-control">
                <label htmlFor="llm-api-key">API Key</label>
                <input
                  id="llm-api-key"
                  type="password"
                  value={settings.llm.apiKey}
                  onChange={(event) => {
                    void applyPatch({ llm: { apiKey: event.target.value } });
                  }}
                />
              </div>
              <div className="filled-control">
                <label htmlFor="llm-model">模型名称</label>
                <input
                  id="llm-model"
                  value={settings.llm.model}
                  onChange={(event) => {
                    void applyPatch({ llm: { model: event.target.value } });
                  }}
                />
              </div>
              <div className="filled-control">
                <label htmlFor="llm-temperature">Temperature</label>
                <input
                  id="llm-temperature"
                  type="number"
                  min={LLM_TEMPERATURE_RANGE.min}
                  max={LLM_TEMPERATURE_RANGE.max}
                  step={0.1}
                  value={getNumberInputValue("llm-temperature", settings.llm.temperature)}
                  onChange={(event) => {
                    setNumberInputValue("llm-temperature", event.target.value);
                  }}
                  onKeyDown={blurNumberInputOnEnter}
                  onBlur={() => {
                    void commitFloatInput(
                      "llm-temperature",
                      settings.llm.temperature,
                      LLM_TEMPERATURE_RANGE.min,
                      LLM_TEMPERATURE_RANGE.max,
                      2,
                      (next) => ({ llm: { temperature: next } })
                    );
                  }}
                />
              </div>
              <div className="settings-inline-actions">
                <p className="inline-action-result" aria-live="polite">
                  {llmApiTestFeedback}
                </p>
                <button
                  type="button"
                  className="path-picker-btn"
                  onClick={() => void testLlmApiConfig()}
                  disabled={testingLlmApi || updating}
                >
                  <Zap size={13} />
                  <span>{testingLlmApi ? "测试中..." : "测试 API 配置"}</span>
                </button>
              </div>
            </article>

          </section>
        )}

        {activeGroup === "smartOcr" && (
          <section className="settings-stack">
            <article className="settings-card">
              <h2>智能 OCR 行为</h2>
              <label className="check-row">
                <span>启用智能 OCR</span>
                <input
                  className="md2-check"
                  type="checkbox"
                  checked={settings.ocr.enabled}
                  onChange={(event) => {
                    void applyPatch({ ocr: { enabled: event.target.checked } });
                  }}
                />
              </label>

              <label className="check-row">
                <span>框选后自动执行处理</span>
                <input
                  className="md2-check"
                  type="checkbox"
                  checked={settings.ocr.autoRunAfterCapture}
                  onChange={(event) => {
                    void applyPatch({ ocr: { autoRunAfterCapture: event.target.checked } });
                  }}
                />
              </label>

              <div className="filled-control">
                <label htmlFor="ocr-default-action">默认处理功能</label>
                <select
                  id="ocr-default-action"
                  className="md2-select"
                  value={ocrActionSelectValue}
                  onChange={(event) => {
                    applyOcrDefaultActionFromSelect(event.target.value);
                  }}
                >
                  {OCR_ACTION_OPTIONS.filter((item) => item.key !== "custom").map((item) => (
                    <option key={item.key} value={item.key}>
                      {item.label}
                    </option>
                  ))}
                  <option value="custom">
                    {ocrCustomActionOptions.length > 0
                      ? "自定义 Agent（未指定）"
                      : "自定义 Agent（请先创建）"}
                  </option>
                  {ocrCustomActionOptions.map((item) => (
                    <option key={item.value} value={item.value}>
                      {item.label}
                    </option>
                  ))}
                </select>
              </div>

              <label className="check-row">
                <span>OCR 结果窗口默认置顶</span>
                <input
                  className="md2-check"
                  type="checkbox"
                  checked={settings.ocr.resultWindowAlwaysOnTop}
                  onChange={(event) => {
                    void applyPatch({ ocr: { resultWindowAlwaysOnTop: event.target.checked } });
                  }}
                />
              </label>
              <p className="help-text">OCR 快捷键：{settings.shortcuts.toggleOcr}</p>
            </article>

            <article className="settings-card">
              <h2>视觉模型 API（OpenAI-compatible / GLM OCR）</h2>
              <div className="filled-control">
                <label htmlFor="ocr-vision-base-url">Base URL</label>
                <input
                  id="ocr-vision-base-url"
                  value={settings.ocr.vision.baseUrl}
                  onChange={(event) => {
                    void applyPatch({
                      ocr: { vision: { baseUrl: event.target.value } }
                    });
                  }}
                />
              </div>

              <div className="filled-control">
                <label htmlFor="ocr-vision-api-key">API Key</label>
                <input
                  id="ocr-vision-api-key"
                  type="password"
                  value={settings.ocr.vision.apiKey}
                  onChange={(event) => {
                    void applyPatch({
                      ocr: {
                        vision: {
                          apiKey: event.target.value
                        }
                      }
                    });
                  }}
                />
              </div>

              <div className="filled-control">
                <label htmlFor="ocr-vision-model">模型名称</label>
                <input
                  id="ocr-vision-model"
                  value={settings.ocr.vision.model}
                  onChange={(event) => {
                    void applyPatch({
                      ocr: { vision: { model: event.target.value } }
                    });
                  }}
                />
              </div>

              <div className="filled-control">
                <label htmlFor="ocr-vision-temperature">Temperature</label>
                <input
                  id="ocr-vision-temperature"
                  type="number"
                  min={LLM_TEMPERATURE_RANGE.min}
                  max={LLM_TEMPERATURE_RANGE.max}
                  step={0.1}
                  value={getNumberInputValue(
                    "ocr-vision-temperature",
                    settings.ocr.vision.temperature
                  )}
                  onChange={(event) => {
                    setNumberInputValue("ocr-vision-temperature", event.target.value);
                  }}
                  onKeyDown={blurNumberInputOnEnter}
                  onBlur={() => {
                    void commitFloatInput(
                      "ocr-vision-temperature",
                      settings.ocr.vision.temperature,
                      LLM_TEMPERATURE_RANGE.min,
                      LLM_TEMPERATURE_RANGE.max,
                      2,
                      (next) => ({ ocr: { vision: { temperature: next } } })
                    );
                  }}
                />
              </div>

              <div className="filled-control">
                <label htmlFor="ocr-vision-max-tokens">Max Tokens</label>
                <input
                  id="ocr-vision-max-tokens"
                  type="number"
                  min={OCR_VISION_MAX_TOKENS_RANGE.min}
                  max={OCR_VISION_MAX_TOKENS_RANGE.max}
                  value={getNumberInputValue(
                    "ocr-vision-max-tokens",
                    settings.ocr.vision.maxTokens
                  )}
                  onChange={(event) => {
                    setNumberInputValue("ocr-vision-max-tokens", event.target.value);
                  }}
                  onKeyDown={blurNumberInputOnEnter}
                  onBlur={() => {
                    void commitIntegerInput(
                      "ocr-vision-max-tokens",
                      settings.ocr.vision.maxTokens,
                      OCR_VISION_MAX_TOKENS_RANGE.min,
                      OCR_VISION_MAX_TOKENS_RANGE.max,
                      (next) => ({ ocr: { vision: { maxTokens: next } } })
                    );
                  }}
                />
              </div>

              <div className="filled-control">
                <label htmlFor="ocr-vision-timeout-ms">Timeout (ms)</label>
                <input
                  id="ocr-vision-timeout-ms"
                  type="number"
                  min={MODEL_TIMEOUT_MS_RANGE.min}
                  max={MODEL_TIMEOUT_MS_RANGE.max}
                  step={1000}
                  value={getNumberInputValue(
                    "ocr-vision-timeout-ms",
                    settings.ocr.vision.timeoutMs
                  )}
                  onChange={(event) => {
                    setNumberInputValue("ocr-vision-timeout-ms", event.target.value);
                  }}
                  onKeyDown={blurNumberInputOnEnter}
                  onBlur={() => {
                    void commitIntegerInput(
                      "ocr-vision-timeout-ms",
                      settings.ocr.vision.timeoutMs,
                      MODEL_TIMEOUT_MS_RANGE.min,
                      MODEL_TIMEOUT_MS_RANGE.max,
                      (next) => ({ ocr: { vision: { timeoutMs: next } } })
                    );
                  }}
                />
              </div>
              <div className="filled-control">
                <label htmlFor="llm-max-tokens">Max Tokens</label>
                <input
                  id="llm-max-tokens"
                  type="number"
                  min={MODEL_MAX_TOKENS_RANGE.min}
                  max={MODEL_MAX_TOKENS_RANGE.max}
                  value={getNumberInputValue("llm-max-tokens", settings.llm.maxTokens)}
                  onChange={(event) => {
                    setNumberInputValue("llm-max-tokens", event.target.value);
                  }}
                  onKeyDown={blurNumberInputOnEnter}
                  onBlur={() => {
                    void commitIntegerInput(
                      "llm-max-tokens",
                      settings.llm.maxTokens,
                      MODEL_MAX_TOKENS_RANGE.min,
                      MODEL_MAX_TOKENS_RANGE.max,
                      (next) => ({ llm: { maxTokens: next } })
                    );
                  }}
                />
              </div>
              <div className="filled-control">
                <label htmlFor="llm-timeout-ms">Timeout (ms)</label>
                <input
                  id="llm-timeout-ms"
                  type="number"
                  min={MODEL_TIMEOUT_MS_RANGE.min}
                  max={MODEL_TIMEOUT_MS_RANGE.max}
                  step={1000}
                  value={getNumberInputValue("llm-timeout-ms", settings.llm.timeoutMs)}
                  onChange={(event) => {
                    setNumberInputValue("llm-timeout-ms", event.target.value);
                  }}
                  onKeyDown={blurNumberInputOnEnter}
                  onBlur={() => {
                    void commitIntegerInput(
                      "llm-timeout-ms",
                      settings.llm.timeoutMs,
                      MODEL_TIMEOUT_MS_RANGE.min,
                      MODEL_TIMEOUT_MS_RANGE.max,
                      (next) => ({ llm: { timeoutMs: next } })
                    );
                  }}
                />
              </div>
              <div className="settings-inline-actions">
                <p className="inline-action-result" aria-live="polite">
                  {ocrApiTestFeedback}
                </p>
                <button
                  type="button"
                  className="path-picker-btn"
                  onClick={() => void testOcrApiConfig()}
                  disabled={testingOcrApi || updating}
                >
                  <Zap size={13} />
                  <span>{testingOcrApi ? "测试中..." : "测试 API 配置"}</span>
                </button>
              </div>
            </article>
          </section>
        )}

        {activeGroup === "tts" && (
          <section className="settings-stack">
            <article className="settings-card">
              <h2>语音播放（Edge TTS）</h2>

              <div className="filled-control">
                <label htmlFor="tts-runtime-mode">调用模式</label>
                <select
                  id="tts-runtime-mode"
                  className="md2-select"
                  value={settings.tts.runtimeMode}
                  onChange={(event) => {
                    void applyPatch({
                      tts: {
                        runtimeMode: parseTtsRuntimeMode(event.target.value)
                      }
                    });
                  }}
                >
                  {TTS_RUNTIME_MODE_OPTIONS.map((item) => (
                    <option key={item.key} value={item.key}>
                      {item.label}
                    </option>
                  ))}
                </select>
              </div>

              <div className="filled-control">
                <label htmlFor="tts-voice-zh">中文 Voice</label>
                <select
                  id="tts-voice-zh"
                  className="md2-select"
                  value={settings.tts.voiceZhCn}
                  onChange={(event) => {
                    void applyPatch({
                      tts: { voiceZhCn: event.target.value }
                    });
                  }}
                >
                  {!TTS_ZH_VOICE_OPTIONS.some((item) => item.key === settings.tts.voiceZhCn) && (
                    <option value={settings.tts.voiceZhCn}>{settings.tts.voiceZhCn}</option>
                  )}
                  {TTS_ZH_VOICE_OPTIONS.map((item) => (
                    <option key={item.key} value={item.key}>
                      {item.label}
                    </option>
                  ))}
                </select>
              </div>

              <div className="filled-control">
                <label htmlFor="tts-voice-en">English Voice</label>
                <select
                  id="tts-voice-en"
                  className="md2-select"
                  value={settings.tts.voiceEnUs}
                  onChange={(event) => {
                    void applyPatch({
                      tts: { voiceEnUs: event.target.value }
                    });
                  }}
                >
                  {!TTS_EN_VOICE_OPTIONS.some((item) => item.key === settings.tts.voiceEnUs) && (
                    <option value={settings.tts.voiceEnUs}>{settings.tts.voiceEnUs}</option>
                  )}
                  {TTS_EN_VOICE_OPTIONS.map((item) => (
                    <option key={item.key} value={item.key}>
                      {item.label}
                    </option>
                  ))}
                </select>
              </div>

              <div className="filled-control">
                <label htmlFor="tts-rate">语速 (%)</label>
                <input
                  id="tts-rate"
                  type="number"
                  min={TTS_RATE_PERCENT_RANGE.min}
                  max={TTS_RATE_PERCENT_RANGE.max}
                  step={1}
                  value={getNumberInputValue("tts-rate", settings.tts.ratePercent)}
                  onChange={(event) => {
                    setNumberInputValue("tts-rate", event.target.value);
                  }}
                  onKeyDown={blurNumberInputOnEnter}
                  onBlur={() => {
                    void commitIntegerInput(
                      "tts-rate",
                      settings.tts.ratePercent,
                      TTS_RATE_PERCENT_RANGE.min,
                      TTS_RATE_PERCENT_RANGE.max,
                      (next) => ({ tts: { ratePercent: next } })
                    );
                  }}
                />
              </div>
              <p className="help-text">首次安装后首次启动会自动安装 edge-tts 运行环境。</p>
            </article>
          </section>
        )}

        {activeGroup === "shortcuts" && (
          <section className="settings-stack">
            <article className="settings-card">
              <h2>全局快捷键</h2>
              <p className="help-text">点击按钮后按下组合键（至少包含 Ctrl/Alt/Shift/Meta）。</p>
              <div className="shortcut-grid">
                <div className={`shortcut-item${recordingMainShortcut ? " recording" : ""}`}>
                  <div className="shortcut-item-row">
                    <div className="shortcut-item-title">
                      <Keyboard size={14} />
                      <div>
                        <h3>粘贴窗口</h3>
                        <p>打开或隐藏剪贴板窗口</p>
                      </div>
                    </div>
                    <button
                      type="button"
                      className={`shortcut-capture-btn${recordingMainShortcut ? " recording" : ""}`}
                      onClick={() => {
                        setRecordingOcrShortcut(false);
                        setRecordingMainShortcut((prev) => !prev);
                        setStatus((prev) =>
                          recordingMainShortcut ? prev : "正在录制粘贴窗口快捷键，按 Esc 可取消"
                        );
                      }}
                      disabled={updating || recordingOcrShortcut}
                    >
                      {recordingMainShortcut ? "录制中... 按 Esc 取消" : settings.shortcuts.toggleMain}
                    </button>
                  </div>
                </div>

                <div className={`shortcut-item${recordingOcrShortcut ? " recording" : ""}`}>
                  <div className="shortcut-item-row">
                    <div className="shortcut-item-title">
                      <ScanSearch size={14} />
                      <div>
                        <h3>智能 OCR</h3>
                        <p>启动 OCR 截图识别流程</p>
                      </div>
                    </div>
                    <button
                      type="button"
                      className={`shortcut-capture-btn${recordingOcrShortcut ? " recording" : ""}`}
                      onClick={() => {
                        setRecordingMainShortcut(false);
                        setRecordingOcrShortcut((prev) => !prev);
                        setStatus((prev) =>
                          recordingOcrShortcut ? prev : "正在录制 OCR 快捷键，按 Esc 可取消"
                        );
                      }}
                      disabled={updating || recordingMainShortcut}
                    >
                      {recordingOcrShortcut ? "录制中... 按 Esc 取消" : settings.shortcuts.toggleOcr}
                    </button>
                  </div>
                </div>
              </div>
            </article>
          </section>
        )}

        {activeGroup === "clipboard" && (
          <section className="settings-stack">
            <article className="settings-card">
              <h2>采集策略</h2>
              <div className="filled-control">
                <label htmlFor="poll-ms">轮询间隔 (ms)</label>
                <input
                  id="poll-ms"
                  type="number"
                  min={POLL_MS_RANGE.min}
                  max={POLL_MS_RANGE.max}
                  step={100}
                  value={getNumberInputValue("poll-ms", settings.history.pollMs)}
                  onChange={(event) => {
                    setNumberInputValue("poll-ms", event.target.value);
                  }}
                  onKeyDown={blurNumberInputOnEnter}
                  onBlur={() => {
                    void commitIntegerInput(
                      "poll-ms",
                      settings.history.pollMs,
                      POLL_MS_RANGE.min,
                      POLL_MS_RANGE.max,
                      (next) => ({ history: { pollMs: next } })
                    );
                  }}
                />
              </div>

              <div className="filled-control">
                <label htmlFor="history-max">历史容量上限</label>
                <input
                  id="history-max"
                  type="number"
                  min={HISTORY_MAX_RANGE.min}
                  max={HISTORY_MAX_RANGE.max}
                  step={10}
                  value={getNumberInputValue("history-max", settings.history.maxItems)}
                  onChange={(event) => {
                    setNumberInputValue("history-max", event.target.value);
                  }}
                  onKeyDown={blurNumberInputOnEnter}
                  onBlur={() => {
                    void commitIntegerInput(
                      "history-max",
                      settings.history.maxItems,
                      HISTORY_MAX_RANGE.min,
                      HISTORY_MAX_RANGE.max,
                      (next) => ({ history: { maxItems: next } })
                    );
                  }}
                />
              </div>

              <div className="filled-control">
                <label htmlFor="default-category">默认分类</label>
                <select
                  id="default-category"
                  className="md2-select"
                  value={settings.history.defaultCategory}
                  onChange={(event) => {
                    void applyPatch({ history: { defaultCategory: parseFilter(event.target.value) } });
                  }}
                >
                  {FILTER_OPTIONS.map((item) => (
                    <option key={item.key} value={item.key}>
                      {item.label}
                    </option>
                  ))}
                </select>
              </div>

              <div className="filled-control">
                <label htmlFor="paste-behavior">点击条目后的行为</label>
                <select
                  id="paste-behavior"
                  className="md2-select"
                  value={settings.history.pasteBehavior}
                  onChange={(event) => {
                    void applyPatch({
                      history: { pasteBehavior: parsePasteBehavior(event.target.value) }
                    });
                  }}
                >
                  {PASTE_BEHAVIOR_OPTIONS.map((item) => (
                    <option key={item.key} value={item.key}>
                      {item.label}
                    </option>
                  ))}
                </select>
              </div>

              <label className="check-row">
                <span>去重合并相同内容</span>
                <input
                  className="md2-check"
                  type="checkbox"
                  checked={settings.history.dedupe}
                  onChange={(event) => {
                    void applyPatch({ history: { dedupe: event.target.checked } });
                  }}
                />
              </label>
              <label className="check-row">
                <span>采集文本</span>
                <input
                  className="md2-check"
                  type="checkbox"
                  checked={settings.history.captureText}
                  onChange={(event) => {
                    void applyPatch({ history: { captureText: event.target.checked } });
                  }}
                />
              </label>
              <label className="check-row">
                <span>采集链接</span>
                <input
                  className="md2-check"
                  type="checkbox"
                  checked={settings.history.captureLink}
                  onChange={(event) => {
                    void applyPatch({ history: { captureLink: event.target.checked } });
                  }}
                />
              </label>
              <label className="check-row">
                <span>采集图片</span>
                <input
                  className="md2-check"
                  type="checkbox"
                  checked={settings.history.captureImage}
                  onChange={(event) => {
                    void applyPatch({ history: { captureImage: event.target.checked } });
                  }}
                />
              </label>
              <label className="check-row">
                <span>折叠顶栏（上滚展开）</span>
                <input
                  className="md2-check"
                  type="checkbox"
                  checked={settings.history.collapseTopBar}
                  onChange={(event) => {
                    void applyPatch({ history: { collapseTopBar: event.target.checked } });
                  }}
                />
              </label>
            </article>
          </section>
        )}

        {activeGroup === "dataBackup" && (
          <section className="settings-stack">
            <article className="settings-card">
              <h2>数据存储位置</h2>
              <div className="filled-control">
                <label htmlFor="history-storage-path">复制内容存储位置（目录或 .json 文件）</label>
                <div className="path-picker-row">
                  <input
                    id="history-storage-path"
                    value={settings.history.storagePath}
                    placeholder="留空使用默认位置"
                    onChange={(event) => {
                      void applyPatch({
                        history: { storagePath: event.target.value }
                      });
                    }}
                  />
                  <button
                    type="button"
                    className="path-picker-btn"
                    onClick={() => void pickHistoryStorageFolder()}
                    disabled={updating || pickingStorageFolder}
                  >
                    <FolderOpen size={13} />
                    <span>{pickingStorageFolder ? "选择中..." : "选择文件夹"}</span>
                  </button>
                </div>
              </div>
              <p className="help-text">
                留空时使用应用默认数据目录；填写目录会自动保存为 {`clipboard_history.json`}。
              </p>
            </article>

            <article className="settings-card">
              <h2>历史维护</h2>
              <div className="card-actions">
                <button className="tonal-btn" onClick={() => void clearHistory()}>
                  <Trash2 size={14} />
                  <span>清理历史</span>
                </button>
                <button className="tonal-btn" onClick={() => void resetAllSettings()}>
                  <RotateCcw size={14} />
                  <span>恢复默认</span>
                </button>
                <button className="tonal-btn" onClick={() => void refresh()}>
                  <RefreshCw size={14} />
                  <span>重新加载</span>
                </button>
              </div>
            </article>

            <article className="settings-card">
              <h2>导出配置</h2>
              <div className="card-actions">
                <button className="tonal-btn" onClick={() => void exportSettingsToTextarea()}>
                  <FileDown size={14} />
                  <span>导出配置</span>
                </button>
                <button className="tonal-btn" onClick={() => void copyExportJson()}>
                  <ExternalLink size={14} />
                  <span>复制 JSON</span>
                </button>
              </div>
              <textarea
                className="settings-json"
                readOnly
                value={exportText}
                placeholder="导出后会显示 JSON 配置"
              />
            </article>

            <article className="settings-card">
              <h2>导入配置</h2>
              <div className="card-actions">
                <button className="tonal-btn" onClick={() => void importSettingsFromTextarea()}>
                  <FileUp size={14} />
                  <span>导入并应用</span>
                </button>
              </div>
              <textarea
                className="settings-json"
                value={importText}
                onChange={(event) => setImportText(event.target.value)}
                placeholder="粘贴 JSON 配置后导入"
              />
            </article>
          </section>
        )}

        {activeGroup === "about" && (
          <section className="settings-stack">
            <article className="settings-card">
              <h2>运行状态</h2>
              <div className="info-grid">
                <p>
                  <History size={14} />
                  <span>轮询间隔：{settings.history.pollMs} ms</span>
                </p>
                <p>
                  <Keyboard size={14} />
                  <span>呼出快捷键：{settings.shortcuts.toggleMain}</span>
                </p>
                <p>
                  <Moon size={14} />
                  <span>主题：{settings.themePreset}</span>
                </p>
              </div>
            </article>

            <article className="settings-card">
              <h2>应用更新</h2>
              <p className="help-text">当前版本：v{appVersion}</p>
              <p className="help-text">
                可用版本：{availableUpdateVersion ? `v${availableUpdateVersion}` : "暂无"}
              </p>
              <div className="card-actions">
                <button
                  className="tonal-btn"
                  onClick={() => void checkForAppUpdates()}
                  disabled={updateChecking || updateInstalling}
                >
                  <RefreshCw size={14} />
                  <span>{updateChecking ? "检查中..." : "检查更新"}</span>
                </button>
                <button
                  className="tonal-btn primary"
                  onClick={() => void downloadAndInstallAppUpdate()}
                  disabled={!availableUpdateVersion || updateChecking || updateInstalling}
                >
                  <Download size={14} />
                  <span>{updateInstalling ? "安装中..." : "下载并安装"}</span>
                </button>
              </div>
              <p className="help-text">{updateStatusText}</p>
              {typeof updateDownloadProgress === "number" && (
                <p className="help-text">下载进度：{updateDownloadProgress}%</p>
              )}
              {availableUpdateNotes && (
                <textarea
                  className="settings-json"
                  readOnly
                  value={availableUpdateNotes}
                  aria-label="更新说明"
                />
              )}
            </article>

            <article className="settings-card">
              <h2>版本信息</h2>
              <p className="help-text">配置版本：v{settings.version}</p>
              <p className="help-text">SnapParse - Tauri v2 + React</p>
            </article>
          </section>
        )}

        {agentModalOpen && (
          <div className="agent-modal-overlay" onClick={closeAgentManager}>
            <section
              className="agent-modal"
              role="dialog"
              aria-modal="true"
              aria-label="自定义 Agent 管理"
              onClick={(event) => event.stopPropagation()}
            >
              <header className="agent-modal-head">
                <div>
                  <h2>自定义 Agent 管理</h2>
                </div>
                <button className="icon-btn" onClick={closeAgentManager} aria-label="关闭">
                  <X size={14} />
                </button>
              </header>

              <div className="agent-modal-body">
                <section className="agent-detail-pane">
                  {activeAgentDraft ? (
                    <>
                      <div className="filled-control">
                        <label htmlFor="agent-name">Agent 名称</label>
                        <input
                          id="agent-name"
                          value={activeAgentDraft.name}
                          onChange={(event) => {
                            setCustomAgentDraft(activeAgentDraft.id, {
                              name: event.target.value
                            });
                          }}
                        />
                      </div>
                      <p className="help-text">
                        长度：{countNameUnits(activeAgentDraft.name)}/{CUSTOM_AGENT_NAME_MAX_UNITS}
                        （中文按 2 计数，最多 4 个汉字或 8 个英文字符）
                      </p>

                      <div className="agent-icon-picker">
                        <p className="help-text">图标库（可视化）</p>
                        <div className="agent-icon-scroll">
                          <div className="agent-icon-grid">
                            {CUSTOM_AGENT_ICON_OPTIONS.map((item) => {
                              const Icon = item.icon;
                              const selected = activeAgentDraft.icon === item.key;
                              return (
                                <button
                                  key={item.key}
                                  className={`agent-icon-option${selected ? " active" : ""}`}
                                  onClick={() => {
                                    setCustomAgentDraft(activeAgentDraft.id, { icon: item.key });
                                  }}
                                  title={item.label}
                                >
                                  <Icon size={13} />
                                  <span>{item.label}</span>
                                </button>
                              );
                            })}
                          </div>
                        </div>
                      </div>

                      <div className="filled-control">
                        <label htmlFor="agent-prompt">Prompt</label>
                        <textarea
                          id="agent-prompt"
                          className="settings-json agent-prompt-input"
                          value={activeAgentDraft.prompt}
                          onChange={(event) => {
                            setCustomAgentDraft(activeAgentDraft.id, {
                              prompt: event.target.value
                            });
                          }}
                        />
                      </div>
                    </>
                  ) : (
                    <p className="help-text">请选择或创建一个 Agent 进行编辑。</p>
                  )}
                </section>
              </div>

              {agentEditorError && <p className="agent-modal-error">{agentEditorError}</p>}

              <footer className="agent-modal-footer">
                <button className="tonal-btn" onClick={closeAgentManager}>
                  取消
                </button>
                <button className="tonal-btn primary" onClick={() => void saveAgentManager()}>
                  保存并应用
                </button>
              </footer>
            </section>
          </div>
        )}
      </section>
    </main>
  );
}

export function App() {
  const settingsApi = useAppSettings();
  const label = getCurrentLabel();

  useEffect(() => {
    document.documentElement.setAttribute("data-window", label);
    return () => {
      document.documentElement.removeAttribute("data-window");
    };
  }, [label]);

  if (label === "settings") {
    return <SettingsWindow settingsApi={settingsApi} />;
  }
  if (label === "selection_bar") {
    return <SelectionBarWindow settingsApi={settingsApi} />;
  }
  if (label === "selection_result") {
    return <SelectionResultWindow settingsApi={settingsApi} />;
  }
  if (label === "ocr_capture") {
    return <OcrCaptureWindow />;
  }
  if (label === "ocr_result") {
    return <OcrResultWindow settingsApi={settingsApi} />;
  }

  return <ClipboardWindow settingsApi={settingsApi} />;
}
