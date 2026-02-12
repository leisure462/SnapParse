import { invoke } from "@tauri-apps/api/core";
import { useThemeMode } from "./windows/theme/themeStore";

type RuntimeWindow = Window & {
  __TAURI_INTERNALS__?: unknown;
};

function isTauriRuntime(): boolean {
  return typeof window !== "undefined" && Boolean((window as RuntimeWindow).__TAURI_INTERNALS__);
}

async function openSettingsWindow(): Promise<void> {
  if (!isTauriRuntime()) {
    return;
  }

  try {
    await invoke("open_window", { kind: "settings" });
  } catch {
    // ignore: settings button should fail silently outside Tauri runtime
  }
}

function App(): JSX.Element {
  const theme = useThemeMode();

  return (
    <main className="app-shell">
      <section className="surface-card">
        <h1 className="app-title">SnapParse</h1>
        <p className="app-subtitle">Tauri v2 Windows 划词助手（主题: {theme.effective}）</p>
        <p className="app-hint">软件常驻后台运行。请点击托盘图标进入设置。</p>

        <div className="app-actions">
          <button
            type="button"
            className="app-btn app-btn-primary"
            onClick={() => {
              void openSettingsWindow();
            }}
          >
            打开设置
          </button>
        </div>
      </section>
    </main>
  );
}

export default App;
