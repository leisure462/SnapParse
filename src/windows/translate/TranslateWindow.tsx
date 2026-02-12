import { listen } from "@tauri-apps/api/event";
import { useEffect, useMemo, useRef, useState } from "react";
import ResultPanel from "../common/ResultPanel";
import WindowHeader from "../common/WindowHeader";
import { useFeatureWindow } from "../common/useFeatureWindow";
import { useStreamingAI } from "../common/useStreamingAI";
import "../common/windowChrome.css";

interface ChangeTextPayload {
  text: string;
  source?: string;
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
  const [fromLanguage, setFromLanguage] = useState("auto");
  const [toLanguage, setToLanguage] = useState("en");
  const fw = useFeatureWindow();
  const ai = useStreamingAI("翻译失败");
  const prevTrigger = useRef<string>("");

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

    // Build a trigger key so we re-run when source/from/to changes
    const triggerKey = `${sourceText}|${fromLanguage}|${toLanguage}`;
    if (triggerKey === prevTrigger.current) {
      return;
    }
    prevTrigger.current = triggerKey;

    ai.startStream("translate", sourceText, {
      fromLanguage,
      toLanguage,
    });
  }, [sourceText, fromLanguage, toLanguage]);

  return (
    <main className="md2-window-shell" style={fw.shellStyle}>
      <section className="md2-window-card">
        <WindowHeader
          title="翻译"
          subtitle={subtitle}
          pinned={fw.pinned}
          onPinToggle={fw.onPinToggle}
        />

        <div className="md2-window-body">
          <section className="md2-lang-bar">
            <select
              className="md2-lang-select"
              value={fromLanguage}
              onChange={(event) => {
                setFromLanguage(event.target.value);
              }}
              aria-label="源语言"
            >
              {LANGUAGES.map((item) => (
                <option key={item.code} value={item.code}>
                  {item.label}
                </option>
              ))}
            </select>

            <span className="md2-lang-arrow" aria-hidden="true">
              <svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <path d="M5 12h14" />
                <path d="m12 5 7 7-7 7" />
              </svg>
            </span>

            <select
              className="md2-lang-select"
              value={toLanguage}
              onChange={(event) => {
                setToLanguage(event.target.value);
              }}
              aria-label="目标语言"
            >
              {LANGUAGES.filter((item) => item.code !== "auto").map((item) => (
                <option key={item.code} value={item.code}>
                  {item.label}
                </option>
              ))}
            </select>
          </section>

          <ResultPanel
            originalText={sourceText}
            resultText={ai.resultText}
            loading={ai.loading}
            streaming={ai.streaming}
            error={ai.errorText}
          />
        </div>
      </section>
    </main>
  );
}
