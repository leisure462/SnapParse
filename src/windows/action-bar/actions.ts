import type { AppSettings, BuiltinActionId } from "../../shared/settings";

export type ActionBarWindowKind = "translate" | "summary" | "explain" | "optimize";

export interface ActionBarAction {
  id: string;
  label: string;
  builtinId?: BuiltinActionId;
  icon: string;
  commandWindow?: ActionBarWindowKind;
  prompt?: string;
  model?: string;
}

const BUILTIN_META: Record<BuiltinActionId, Omit<ActionBarAction, "id">> = {
  translate: { label: "翻译", builtinId: "translate", icon: "translate", commandWindow: "translate" },
  explain: { label: "解释", builtinId: "explain", icon: "explain", commandWindow: "explain" },
  summarize: { label: "总结", builtinId: "summarize", icon: "summarize", commandWindow: "summary" },
  optimize: { label: "优化", builtinId: "optimize", icon: "optimize", commandWindow: "optimize" },
  search: { label: "搜索", builtinId: "search", icon: "search" },
  copy: { label: "复制", builtinId: "copy", icon: "copy" }
};

export function resolveActionBarActions(settings: AppSettings): ActionBarAction[] {
  const builtin = [...settings.toolbar.actions]
    .filter((item) => item.enabled)
    .sort((a, b) => a.order - b.order)
    .map((item) => ({
      id: item.id,
      ...BUILTIN_META[item.id]
    }));

  const custom = [...settings.features.customActions]
    .filter((item) => item.enabled)
    .sort((a, b) => a.order - b.order)
    .map((item) => ({
      id: item.id,
      label: item.name,
      icon: item.icon,
      commandWindow: "optimize" as const,
      prompt: item.prompt,
      model: item.model
    }));

  return [...builtin, ...custom];
}
