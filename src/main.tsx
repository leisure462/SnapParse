import React from "react";
import ReactDOM from "react-dom/client";
import App from "./App";
import "./styles/global.css";
import { resolveWindowFromLocation, resolveWindowFromRuntime } from "./windows/router";
import "./windows/theme/themeTokens.css";
import ActionBarWindow from "./windows/action-bar/ActionBarWindow";
import TranslateWindow from "./windows/translate/TranslateWindow";
import SummaryWindow from "./windows/summary/SummaryWindow";
import SettingsWindow from "./windows/settings/SettingsWindow";
import ExplainWindow from "./windows/explain/ExplainWindow";
import OptimizeWindow from "./windows/optimize/OptimizeWindow";

function WindowPlaceholder(props: { windowKey: string }): JSX.Element {
  return (
    <main className="app-shell">
      <section className="surface-card">
        <h1 className="app-title">SnapParse</h1>
        <p className="app-subtitle">{props.windowKey} 窗口骨架已挂载</p>
      </section>
    </main>
  );
}

function resolveAppEntry(): JSX.Element {
  const key = resolveWindowFromRuntime() ?? resolveWindowFromLocation(window.location.search);

  if (key === "main") {
    return <App />;
  }

  if (key === "action-bar") {
    return <ActionBarWindow />;
  }

  if (key === "translate") {
    return <TranslateWindow />;
  }

  if (key === "summary") {
    return <SummaryWindow />;
  }

  if (key === "explain") {
    return <ExplainWindow />;
  }

  if (key === "optimize") {
    return <OptimizeWindow />;
  }

  if (key === "settings") {
    return <SettingsWindow />;
  }

  return <WindowPlaceholder windowKey={key} />;
}

ReactDOM.createRoot(document.getElementById("root") as HTMLElement).render(
  <React.StrictMode>
    {resolveAppEntry()}
  </React.StrictMode>
);
