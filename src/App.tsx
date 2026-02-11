import { invoke } from "@tauri-apps/api/core";
import { useEffect, useState } from "react";
import type { AppSettings } from "./shared/settings";
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
  const [hintText, setHintText] = useState("首次使用请先打开设置配置 API。\n划词工具栏默认显示在选区上方。 ");

  useEffect(() => {
    if (!isTauriRuntime()) {
      return;
    }

    let cancelled = false;

    const bootstrap = async (): Promise<void> => {
      try {
        const settings = await invoke<AppSettings>("get_settings");
        if (cancelled) {
          return;
        }

        const hasApiKey = Boolean(settings?.api?.apiKey?.trim());

        if (!hasApiKey) {
          await openSettingsWindow();
          if (!cancelled) {
            setHintText("已自动打开设置窗口，请先填写 API 配置后再划词使用。");
          }
        } else {
          setHintText("API 已配置，可直接在任意应用划词触发工具栏。 ");
        }
      } catch {
        if (!cancelled) {
          setHintText("无法读取设置，点击下方按钮打开设置窗口进行配置。 ");
        }
      }
    };

    void bootstrap();

    return () => {
      cancelled = true;
    };
  }, []);

  return (
    <main className="app-shell">
      <section className="surface-card">
        <h1 className="app-title">SnapParse</h1>
        <p className="app-subtitle">Tauri v2 Windows 划词助手（主题: {theme.effective}）</p>
        <p className="app-hint">{hintText}</p>

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
