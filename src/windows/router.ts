export type WindowRouteKey =
  | "main"
  | "action-bar"
  | "translate"
  | "summary"
  | "explain"
  | "settings";

const WINDOW_ROUTE_MAP: Record<WindowRouteKey, string> = {
  main: "/",
  "action-bar": "/windows/action-bar",
  translate: "/windows/translate",
  summary: "/windows/summary",
  explain: "/windows/explain",
  settings: "/windows/settings"
};

export function resolveWindowRoute(key: WindowRouteKey): string {
  return WINDOW_ROUTE_MAP[key];
}

export function resolveWindowFromLocation(search: string): WindowRouteKey {
  const params = new URLSearchParams(search);
  const target = params.get("window");

  if (!target) {
    return "main";
  }

  if (target in WINDOW_ROUTE_MAP) {
    return target as WindowRouteKey;
  }

  return "main";
}

type RuntimeWindow = Window & {
  __SNAPPARSE_WINDOW_KIND?: string;
};

export function resolveWindowFromRuntime(): WindowRouteKey | undefined {
  const runtimeWindow = window as RuntimeWindow;
  const key = runtimeWindow.__SNAPPARSE_WINDOW_KIND;

  if (!key) {
    return undefined;
  }

  if (key in WINDOW_ROUTE_MAP) {
    return key as WindowRouteKey;
  }

  return undefined;
}
