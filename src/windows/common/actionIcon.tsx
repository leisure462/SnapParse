export interface ActionIconPreset {
  id: string;
  label: string;
}

export const CUSTOM_ACTION_ICON_PRESETS: ActionIconPreset[] = [
  { id: "sparkles", label: "闪光" },
  { id: "rocket", label: "火箭" },
  { id: "wand", label: "魔杖" },
  { id: "bolt", label: "闪电" },
  { id: "atom", label: "原子" },
  { id: "beaker", label: "烧杯" },
  { id: "pen", label: "钢笔" },
  { id: "book", label: "书籍" },
  { id: "target", label: "靶心" },
  { id: "light", label: "灯泡" },
  { id: "leaf", label: "叶子" },
  { id: "flame", label: "火焰" },
  { id: "drop", label: "水滴" },
  { id: "wave", label: "波纹" },
  { id: "cloud", label: "云朵" },
  { id: "sun", label: "太阳" },
  { id: "moon", label: "月亮" },
  { id: "star", label: "星星" },
  { id: "planet", label: "行星" },
  { id: "compass", label: "指南针" },
  { id: "map", label: "地图" },
  { id: "code", label: "代码" },
  { id: "terminal", label: "终端" },
  { id: "chip", label: "芯片" },
  { id: "cpu", label: "处理器" },
  { id: "database", label: "数据库" },
  { id: "folder", label: "文件夹" },
  { id: "file", label: "文件" },
  { id: "link", label: "链接" },
  { id: "globe", label: "地球" },
  { id: "search", label: "搜索" },
  { id: "filter", label: "筛选" },
  { id: "shield", label: "护盾" },
  { id: "lock", label: "锁" },
  { id: "key", label: "钥匙" },
  { id: "camera", label: "相机" },
  { id: "image", label: "图片" },
  { id: "video", label: "视频" },
  { id: "music", label: "音乐" },
  { id: "mic", label: "麦克风" },
  { id: "message", label: "消息" },
  { id: "chat", label: "聊天" },
  { id: "mail", label: "邮件" },
  { id: "calendar", label: "日历" },
  { id: "clock", label: "时钟" },
  { id: "flag", label: "旗帜" },
  { id: "check", label: "勾选" },
  { id: "heart", label: "爱心" },
  { id: "gift", label: "礼物" },
  { id: "tools", label: "工具" }
];

const CUSTOM_ICON_LABEL_MAP = new Map(CUSTOM_ACTION_ICON_PRESETS.map((item) => [item.id, item.label]));

function fallbackVariant(icon: string): number {
  let hash = 0;
  for (const char of icon) {
    hash = (hash * 33 + char.charCodeAt(0)) >>> 0;
  }
  return hash % 10;
}

const CUSTOM_ICON_VARIANTS: Record<string, number> = {
  sparkles: 0,
  star: 0,
  rocket: 1,
  planet: 1,
  wand: 2,
  pen: 2,
  bolt: 3,
  light: 3,
  atom: 4,
  beaker: 4,
  chip: 4,
  cpu: 4,
  book: 5,
  file: 5,
  folder: 5,
  code: 5,
  terminal: 5,
  target: 6,
  compass: 6,
  map: 6,
  leaf: 7,
  flame: 7,
  drop: 7,
  wave: 7,
  cloud: 7,
  sun: 8,
  moon: 8,
  globe: 8,
  search: 9,
  filter: 9,
  shield: 9,
  lock: 9,
  key: 9,
  camera: 6,
  image: 5,
  video: 5,
  music: 2,
  mic: 2,
  message: 5,
  chat: 5,
  mail: 5,
  calendar: 5,
  clock: 6,
  flag: 6,
  check: 0,
  heart: 0,
  gift: 1,
  tools: 4,
  link: 9,
  database: 4
};

function renderCustomVariant(variant: number, size: number): JSX.Element {
  const common = { viewBox: "0 0 24 24", width: size, height: size, fill: "none", stroke: "currentColor" };

  if (variant === 0) {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <path d="M12 3v4" />
        <path d="M12 17v4" />
        <path d="M3 12h4" />
        <path d="M17 12h4" />
        <path d="m6 6 2.6 2.6" />
        <path d="m15.4 15.4 2.6 2.6" />
        <path d="m18 6-2.6 2.6" />
        <path d="m8.6 15.4-2.6 2.6" />
      </svg>
    );
  }

  if (variant === 1) {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <path d="m6 14 4-4 8-2-2 8-4 4" />
        <path d="m10 10 4 4" />
        <path d="m5 19 3-3" />
      </svg>
    );
  }

  if (variant === 2) {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <path d="m6 18 7-7" />
        <path d="m13 11 2-2" />
        <path d="m15 9 3 3" />
        <path d="m4 20 4-1-3-3z" />
      </svg>
    );
  }

  if (variant === 3) {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <path d="m13 3-7 9h5l-1 9 8-11h-5z" />
      </svg>
    );
  }

  if (variant === 4) {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <circle cx="12" cy="12" r="2" />
        <path d="M4 12h4" />
        <path d="M16 12h4" />
        <path d="M12 4v4" />
        <path d="M12 16v4" />
        <rect x="6" y="6" width="12" height="12" rx="2" />
      </svg>
    );
  }

  if (variant === 5) {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <rect x="4" y="4" width="16" height="16" rx="2" />
        <path d="M8 9h8" />
        <path d="M8 13h8" />
        <path d="M8 17h5" />
      </svg>
    );
  }

  if (variant === 6) {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <circle cx="12" cy="12" r="8" />
        <circle cx="12" cy="12" r="3" />
        <path d="M12 4v2" />
        <path d="M12 18v2" />
      </svg>
    );
  }

  if (variant === 7) {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <path d="M12 4c4 3 6 6 6 9a6 6 0 0 1-12 0c0-3 2-6 6-9z" />
      </svg>
    );
  }

  if (variant === 8) {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <circle cx="12" cy="12" r="5" />
        <path d="M3 12h2" />
        <path d="M19 12h2" />
        <path d="M12 3v2" />
        <path d="M12 19v2" />
      </svg>
    );
  }

  return (
    <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <rect x="5" y="5" width="14" height="14" rx="3" />
      <path d="m9 12 2 2 4-4" />
    </svg>
  );
}

export function resolveCustomIconLabel(iconId: string): string {
  return CUSTOM_ICON_LABEL_MAP.get(iconId) ?? iconId;
}

export function renderActionIcon(icon: string, size = 16): JSX.Element {
  const normalized = icon.trim().toLowerCase();
  const common = { viewBox: "0 0 24 24", width: size, height: size, fill: "none", stroke: "currentColor" };

  if (normalized === "translate") {
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

  if (normalized === "explain") {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <path d="M12 18h.01" />
        <path d="M9 9a3 3 0 1 1 6 0c0 2-3 2-3 5" />
        <rect x="3" y="3" width="18" height="18" rx="3" />
      </svg>
    );
  }

  if (normalized === "summarize") {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <path d="M5 6h14" />
        <path d="M5 12h10" />
        <path d="M5 18h6" />
      </svg>
    );
  }

  if (normalized === "optimize") {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <path d="m7 16 4-9 6 10" />
        <path d="M4 19h16" />
        <path d="m14 5 1.5 1.5L17 5l-1.5-1.5z" />
      </svg>
    );
  }

  if (normalized === "search") {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <circle cx="11" cy="11" r="7" />
        <path d="m20 20-3.8-3.8" />
      </svg>
    );
  }

  if (normalized === "copy") {
    return (
      <svg {...common} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <rect x="8" y="8" width="11" height="12" rx="2" />
        <path d="M5 15V6a2 2 0 0 1 2-2h8" />
      </svg>
    );
  }

  const variant = CUSTOM_ICON_VARIANTS[normalized] ?? fallbackVariant(normalized);
  return renderCustomVariant(variant, size);
}
