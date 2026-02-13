export interface ActionIconPreset {
  id: string;
  label: string;
}

const LUCIDE_ICON_MODULES = import.meta.glob("../../../lucide-common-100-pack/icons/*.svg", {
  eager: true,
  import: "default"
}) as Record<string, string>;

const LUCIDE_ICON_URL_MAP = new Map<string, string>();

for (const [path, url] of Object.entries(LUCIDE_ICON_MODULES)) {
  const filename = path.split("/").pop();
  if (!filename) {
    continue;
  }

  const iconId = filename.replace(/\.svg$/i, "");
  LUCIDE_ICON_URL_MAP.set(iconId, url);
}

function formatLucideLabel(iconId: string): string {
  return iconId
    .split("-")
    .filter(Boolean)
    .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
    .join(" ");
}

export const CUSTOM_ACTION_ICON_PRESETS: ActionIconPreset[] = Array.from(LUCIDE_ICON_URL_MAP.keys())
  .sort((a, b) => a.localeCompare(b))
  .map((id) => ({
    id,
    label: formatLucideLabel(id)
  }));

const CUSTOM_ICON_LABEL_MAP = new Map(CUSTOM_ACTION_ICON_PRESETS.map((item) => [item.id, item.label]));

function renderBuiltinIcon(icon: string, size: number): JSX.Element | null {
  const common = { viewBox: "0 0 24 24", width: size, height: size, fill: "none", stroke: "currentColor" };

  if (icon === "translate") {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <path d="M4 5h10" />
        <path d="M9 5c0 6-3 9-6 11" />
        <path d="M9 11c1.5 2.2 3.3 4 5.2 5.4" />
        <path d="M14 5h6" />
        <path d="M18 5v10" />
      </svg>
    );
  }

  if (icon === "explain") {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <path d="M12 18h.01" />
        <path d="M9 9a3 3 0 1 1 6 0c0 2-3 2-3 5" />
        <rect x="3" y="3" width="18" height="18" rx="3" />
      </svg>
    );
  }

  if (icon === "summarize") {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <path d="M5 6h14" />
        <path d="M5 12h10" />
        <path d="M5 18h6" />
      </svg>
    );
  }

  if (icon === "optimize") {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <path d="m7 16 4-9 6 10" />
        <path d="M4 19h16" />
        <path d="m14 5 1.5 1.5L17 5l-1.5-1.5z" />
      </svg>
    );
  }

  if (icon === "search") {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <circle cx="11" cy="11" r="7" />
        <path d="m20 20-3.8-3.8" />
      </svg>
    );
  }

  if (icon === "copy") {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <rect x="8" y="8" width="11" height="12" rx="2" />
        <path d="M5 15V6a2 2 0 0 1 2-2h8" />
      </svg>
    );
  }

  return null;
}

function renderLucidePackIcon(icon: string, size: number): JSX.Element | null {
  const url = LUCIDE_ICON_URL_MAP.get(icon);
  if (!url) {
    return null;
  }

  return <img src={url} alt="" width={size} height={size} draggable={false} loading="lazy" decoding="async" />;
}

export function resolveCustomIconLabel(iconId: string): string {
  return CUSTOM_ICON_LABEL_MAP.get(iconId) ?? formatLucideLabel(iconId);
}

export function renderActionIcon(icon: string, size = 16): JSX.Element {
  const normalized = icon.trim().toLowerCase();
  const builtin = renderBuiltinIcon(normalized, size);
  if (builtin) {
    return builtin;
  }

  const lucideIcon = renderLucidePackIcon(normalized, size);
  if (lucideIcon) {
    return lucideIcon;
  }

  return (
    <svg viewBox="0 0 24 24" width={size} height={size} fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <circle cx="12" cy="12" r="7" />
      <path d="m9 12 2 2 4-4" />
    </svg>
  );
}
