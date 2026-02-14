import { listen } from "@tauri-apps/api/event";
import { useEffect, useState } from "react";
import ResultPanel from "../common/ResultPanel";
import WindowHeader from "../common/WindowHeader";
import { useFeatureWindow } from "../common/useFeatureWindow";
import { useStreamingAI } from "../common/useStreamingAI";
import "../common/windowChrome.css";

interface ChangeTextPayload {
  text: string;
  source?: string;
  target?: "translate" | "summary" | "explain" | "optimize";
  requestId?: number;
  ocrStage?: "ocring" | "processing" | "idle";
}

export default function SummaryWindow(): JSX.Element {
  const [sourceText, setSourceText] = useState("");
  const [requestId, setRequestId] = useState(0);
  const [ocrLoadingLabel, setOcrLoadingLabel] = useState<string | undefined>(undefined);
  const fw = useFeatureWindow();
  const ai = useStreamingAI("总结失败");

  useEffect(() => {
    let unlisten: (() => void) | undefined;

    listen<ChangeTextPayload>("change-text", (event) => {
      if (event.payload.target && event.payload.target !== "summary") {
        return;
      }

      if (event.payload.ocrStage === "ocring") {
        ai.reset();
        setSourceText("");
        setRequestId(0);
        setOcrLoadingLabel("OCR中...");
        return;
      }

      if (event.payload.ocrStage === "processing") {
        setOcrLoadingLabel("处理中...");
      } else if (event.payload.ocrStage === "idle") {
        setOcrLoadingLabel(undefined);
        return;
      } else {
        setOcrLoadingLabel(undefined);
      }

      if (typeof event.payload.text === "string") {
        setSourceText(event.payload.text);
        setRequestId(event.payload.requestId ?? Date.now());
      }
    }).then((cleanup) => {
      unlisten = cleanup;
    });

    return () => {
      unlisten?.();
    };
  }, []);

  useEffect(() => {
    if (!sourceText.trim() || requestId === 0) {
      return;
    }

    ai.startStream("summarize", sourceText, {
      targetLength: "short",
      language: fw.language
    });
  }, [fw.language, requestId, sourceText]);

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
            loading={ai.loading || (Boolean(ocrLoadingLabel) && !ai.streaming && !ai.resultText.trim() && !ai.errorText)}
            loadingLabel={ocrLoadingLabel}
            streaming={ai.streaming}
            error={ai.errorText}
          />
        </div>
      </section>
    </main>
  );
}
