import { getCurrentWindow } from "@tauri-apps/api/window";

interface WindowHeaderProps {
  title: string;
  subtitle?: string;
  pinned?: boolean;
  onPinToggle?: () => void;
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
        {/* Pin / 置顶 */}
        <button
          type="button"
          className={`md2-window-icon-btn ${props.pinned ? "active" : ""}`}
          onClick={props.onPinToggle}
          aria-pressed={props.pinned}
          aria-label="置顶"
          title={props.pinned ? "取消置顶" : "置顶窗口"}
        >
          <svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
            {props.pinned ? (
              <>
                <path d="M9 4v6l-2 4v2h10v-2l-2-4V4" />
                <line x1="12" y1="16" x2="12" y2="21" />
                <line x1="8" y1="4" x2="16" y2="4" />
              </>
            ) : (
              <>
                <path d="M9 4v6l-2 4v2h10v-2l-2-4V4" />
                <line x1="12" y1="16" x2="12" y2="21" />
                <line x1="8" y1="4" x2="16" y2="4" />
              </>
            )}
          </svg>
        </button>

        {/* Minimize / 最小化 */}
        <button
          type="button"
          className="md2-window-icon-btn"
          onClick={() => {
            void minimizeWindow();
          }}
          aria-label="最小化"
          title="最小化"
        >
          <svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" strokeWidth="2">
            <path d="M6 12h12" />
          </svg>
        </button>

        {/* Close / 关闭 */}
        <button
          type="button"
          className="md2-window-icon-btn danger"
          onClick={() => {
            void closeWindow();
          }}
          aria-label="关闭"
          title="关闭"
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
