import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import { useEffect, useMemo, useRef, useState } from "react";
import ResultPanel from "../common/ResultPanel";
import WindowHeader from "../common/WindowHeader";
import "../common/windowChrome.css";
import "./translate.css";

interface ChangeTextPayload {
  text: string;
  source?: string;
}

interface ProcessTextResponse {
  taskKind: "translate" | "summarize" | "explain";
  sourceText: string;
  resultText: string;
  usedModel: string;
  elapsedMs: number;
}

const LAST_SELECTED_TEXT_KEY = "snapparse:selected-text";

const LANGUAGES: Array<{ code: string; label: string }> = [
  { code: "auto", label: "自动检测" },
  { code: "zh-CN", label: "简体中文" },
  { code: "en", label: "英文" },
  { code: "ja", label: "日文" },
  { code: "ko", label: "韩文" }
];

function findLanguageLabel(code: string): string {
  return LANGUAGES.find((item) => item.code === code)?.label ?? code;
}

export default function TranslateWindow(): JSX.Element {
  const [sourceText, setSourceText] = useState("");
  const [resultText, setResultText] = useState("");
  const [errorText, setErrorText] = useState<string | undefined>();
  const [loading, setLoading] = useState(false);
  const [fromLanguage, setFromLanguage] = useState("auto");
  const [toLanguage, setToLanguage] = useState("en");
  const requestId = useRef(0);

  const subtitle = useMemo(() => {
    return `${findLanguageLabel(fromLanguage)} -> ${findLanguageLabel(toLanguage)}`;
  }, [fromLanguage, toLanguage]);

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
          taskKind: "translate",
          text: sourceText,
          options: {
            fromLanguage,
            toLanguage
          }
        });

        if (currentRequestId !== requestId.current) {
          return;
        }

        setResultText(response.resultText || "");
      } catch (error) {
        if (currentRequestId !== requestId.current) {
          return;
        }

        const message = error instanceof Error ? error.message : String(error);
        setErrorText(`翻译失败：${message}`);
      } finally {
        if (currentRequestId === requestId.current) {
          setLoading(false);
        }
      }
    };

    void run();
  }, [sourceText, fromLanguage, toLanguage]);

  return (
    <main className="md2-window-shell translate-window">
      <section className="md2-window-card">
        <WindowHeader title="翻译" subtitle={subtitle} />

        <div className="md2-window-body">
          <section className="md2-inline-controls">
            <label className="md2-input-group">
              <span className="md2-input-label">源语言</span>
              <select
                className="md2-select"
                value={fromLanguage}
                onChange={(event) => {
                  setFromLanguage(event.target.value);
                }}
              >
                {LANGUAGES.map((item) => (
                  <option key={item.code} value={item.code}>
                    {item.label}
                  </option>
                ))}
              </select>
            </label>

            <span className="md2-inline-arrow" aria-hidden="true">
              {"->"}
            </span>

            <label className="md2-input-group">
              <span className="md2-input-label">目标语言</span>
              <select
                className="md2-select"
                value={toLanguage}
                onChange={(event) => {
                  setToLanguage(event.target.value);
                }}
              >
                {LANGUAGES.filter((item) => item.code !== "auto").map((item) => (
                  <option key={item.code} value={item.code}>
                    {item.label}
                  </option>
                ))}
              </select>
            </label>
          </section>

          <label className="md2-input-group">
            <span className="md2-input-label">原文</span>
            <textarea
              className="md2-textarea"
              value={sourceText}
              onChange={(event) => {
                setSourceText(event.target.value);
              }}
              placeholder="划词后文本会自动注入，也可手动输入"
            />
          </label>

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
