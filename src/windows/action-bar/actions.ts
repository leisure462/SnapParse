export type ActionBarActionId = "translate" | "explain" | "summarize" | "search" | "copy";

export interface ActionBarAction {
  id: ActionBarActionId;
  label: string;
  commandWindow?: "translate" | "summary" | "explain";
}

export const DEFAULT_ACTIONS: ActionBarAction[] = [
  { id: "translate", label: "翻译", commandWindow: "translate" },
  { id: "explain", label: "解释", commandWindow: "explain" },
  { id: "summarize", label: "总结", commandWindow: "summary" },
  { id: "search", label: "搜索" },
  { id: "copy", label: "复制" }
];
