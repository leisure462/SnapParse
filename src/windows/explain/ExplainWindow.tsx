import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import { useEffect, useRef, useState } from "react";
import { defaultSettings, type AppSettings } from "../../shared/settings";
import ResultPanel from "../common/ResultPanel";
import WindowHeader from "../common/WindowHeader";
import "../common/windowChrome.css";

interface ChangeTextPayload {
  text: string;
}

interface ProcessTextResponse {
  resultText: string;
}

const LAST_SELECTED_TEXT_KEY = "snapparse:selected-text";

export default function ExplainWindow(): JSX.Element {
  const [sourceText, setSourceText] = useState("");
  const [resultText, setResultText] = useState("");
  const [loading, setLoading] = useState(false);
  const [errorText, setErrorText] = useState<string | undefined>();
  const [fontSize, setFontSize] = useState(14);
  const requestId = useRef(0);

  useEffect(() => {
    const cached = window.localStorage.getItem(LAST_SELECTED_TEXT_KEY);
    if (cached?.trim()) {
      setSourceText(cached);
    }

    let unlisten: (() => void) | undefined;

    listen<ChangeTextPayload>("change-text", (event) => {
      if (typeof event.payload.text === "string") {
        setSourceText(event.payload.text);
      }
    }).then((cleanup) => {
      unlisten = cleanup;
    });

    return () => {
      unlisten?.();
    };
  }, []);

  useEffect(() => {
    invoke<AppSettings>("get_settings")
      .then((s) => { setFontSize(s.window?.fontSize ?? defaultSettings().window.fontSize); })
      .catch(() => { /* use default */ });
  }, []);

  useEffect(() => {
    if (!sourceText.trim()) {
      return;
    }

    requestId.current += 1;
    const currentRequestId = requestId.current;

    const run = async (): Promise<void> => {
      setLoading(true);
      setErrorText(undefined);

      try {
        const response = await invoke<ProcessTextResponse>("process_selected_text", {
          taskKind: "explain",
          text: sourceText,
          options: {}
        });

        if (currentRequestId === requestId.current) {
          setResultText(response.resultText || "");
        }
      } catch (error) {
        if (currentRequestId === requestId.current) {
          const message = error instanceof Error ? error.message : String(error);
          setErrorText(`解释失败：${message}`);
        }
      } finally {
        if (currentRequestId === requestId.current) {
          setLoading(false);
        }
      }
    };

    void run();
  }, [sourceText]);

  return (
    <main className="md2-window-shell" style={{ "--snapparse-font-size": `${fontSize}px` } as React.CSSProperties}>
      <section className="md2-window-card">
        <WindowHeader title="解释" />

        <div className="md2-window-body">
          <ResultPanel
            originalText={sourceText}
            resultText={resultText}
            loading={loading}
            error={errorText}
          />
        </div>
      </section>
    </main>
  );
}
