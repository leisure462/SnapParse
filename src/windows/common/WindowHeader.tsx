import { getCurrentWindow } from "@tauri-apps/api/window";

interface WindowHeaderProps {
  title: string;
  subtitle?: string;
  pinned?: boolean;
  onPinToggle?: () => void;
  onOpacityCycle?: () => void;
}

async function minimizeWindow(): Promise<void> {
  try {
    await getCurrentWindow().minimize();
  } catch {
    // noop in browser tests
  }
}

async function closeWindow(): Promise<void> {
  try {
    await getCurrentWindow().hide();
  } catch {
    // noop in browser tests
  }
}

export default function WindowHeader(props: WindowHeaderProps): JSX.Element {
  return (
    <header className="md2-window-header" role="banner">
      <div className="md2-window-title-block">
        <h1 className="md2-window-title">{props.title}</h1>
        {props.subtitle ? <p className="md2-window-subtitle">{props.subtitle}</p> : null}
      </div>

      <div className="md2-window-controls" aria-label="窗口控制">
        <button
          type="button"
          className="md2-window-icon-btn"
          onClick={props.onPinToggle}
          aria-pressed={props.pinned}
          aria-label="置顶"
        >
          <svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" strokeWidth="2">
            <path d="M7 4h10" />
            <path d="M12 4v7" />
            <path d="m8 11 4 4 4-4" />
            <path d="M12 15v5" />
          </svg>
        </button>

        <button
          type="button"
          className="md2-window-icon-btn"
          onClick={props.onOpacityCycle}
          aria-label="透明度"
        >
          <svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" strokeWidth="2">
            <path d="M12 3 4 14a8 8 0 1 0 16 0L12 3Z" />
          </svg>
        </button>

        <button
          type="button"
          className="md2-window-icon-btn"
          onClick={() => {
            void minimizeWindow();
          }}
          aria-label="最小化"
        >
          <svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" strokeWidth="2">
            <path d="M6 12h12" />
          </svg>
        </button>

        <button
          type="button"
          className="md2-window-icon-btn danger"
          onClick={() => {
            void closeWindow();
          }}
          aria-label="关闭"
        >
          <svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" strokeWidth="2">
            <path d="m6 6 12 12" />
            <path d="m18 6-12 12" />
          </svg>
        </button>
      </div>
    </header>
  );
}
