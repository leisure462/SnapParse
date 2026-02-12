import { listen } from "@tauri-apps/api/event";
import { useEffect, useRef, useState } from "react";
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

export default function SummaryWindow(): JSX.Element {
  const [sourceText, setSourceText] = useState("");
  const fw = useFeatureWindow();
  const ai = useStreamingAI("总结失败");
  const prevTrigger = useRef<string>("");

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

    if (sourceText === prevTrigger.current) {
      return;
    }
    prevTrigger.current = sourceText;

    ai.startStream("summarize", sourceText, {
      targetLength: "short",
    });
  }, [sourceText]);

  return (
    <main className="md2-window-shell" style={fw.shellStyle}>
      <section className="md2-window-card">
        <WindowHeader
          title="总结"
          pinned={fw.pinned}
          onPinToggle={fw.onPinToggle}
        />

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
