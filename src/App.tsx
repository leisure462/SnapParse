import { useThemeMode } from "./windows/theme/themeStore";

function App(): JSX.Element {
  const theme = useThemeMode();

  return (
    <main className="app-shell">
      <section className="surface-card">
        <h1 className="app-title">SnapParse</h1>
        <p className="app-subtitle">Tauri v2 Windows 划词助手构建中（主题: {theme.effective}）</p>
      </section>
    </main>
  );
}

export default App;
