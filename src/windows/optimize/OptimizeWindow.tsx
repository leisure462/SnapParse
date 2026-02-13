import { listen } from "@tauri-apps/api/event";
import { useEffect, useRef, useState } from "react";
import ResultPanel from "../common/ResultPanel";
import WindowHeader from "../common/WindowHeader";
import { useFeatureWindow } from "../common/useFeatureWindow";
import { useStreamingAI } from "../common/useStreamingAI";
import "../common/windowChrome.css";

interface ChangeTextPayload {
  text: string;
  target?: "translate" | "summary" | "explain" | "optimize";
  title?: string;
  customPrompt?: string;
}

const LAST_SELECTED_TEXT_KEY = "snapparse:selected-text";

export default function OptimizeWindow(): JSX.Element {
  const [sourceText, setSourceText] = useState("");
  const [title, setTitle] = useState("优化");
  const [customPrompt, setCustomPrompt] = useState<string | undefined>(undefined);
  const fw = useFeatureWindow();
  const ai = useStreamingAI("优化失败");
  const prevTrigger = useRef<string>("");

  useEffect(() => {
    const cached = window.localStorage.getItem(LAST_SELECTED_TEXT_KEY);
    if (cached?.trim()) {
      setSourceText(cached);
    }

    let unlisten: (() => void) | undefined;

    listen<ChangeTextPayload>("change-text", (event) => {
      if (event.payload.target && event.payload.target !== "optimize") {
        return;
      }

      if (typeof event.payload.title === "string" && event.payload.title.trim()) {
        setTitle(event.payload.title.trim());
      } else {
        setTitle("优化");
      }

      setCustomPrompt(event.payload.customPrompt?.trim() || undefined);

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

    const triggerKey = `${title}|${customPrompt ?? ""}|${sourceText}`;
    if (triggerKey === prevTrigger.current) {
      return;
    }
    prevTrigger.current = triggerKey;

    ai.startStream("optimize", sourceText, {
      customPrompt,
      language: fw.language
    });
  }, [ai, customPrompt, fw.language, sourceText, title]);

  return (
    <main className="md2-window-shell" style={fw.shellStyle}>
      <section className="md2-window-card">
        <WindowHeader title={title} pinned={fw.pinned} onPinToggle={fw.onPinToggle} />

        <div className="md2-window-body">
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
