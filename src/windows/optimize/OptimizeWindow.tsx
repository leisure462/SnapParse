import { listen } from "@tauri-apps/api/event";
import { useEffect, useState } from "react";
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
  customModel?: string;
  requestId?: number;
}

const LAST_OPTIMIZE_REQUEST_KEY = "snapparse:last-optimize-request";

function safeParsePayload(raw: string | null): ChangeTextPayload | null {
  if (!raw) {
    return null;
  }

  try {
    const parsed = JSON.parse(raw) as ChangeTextPayload;
    if (typeof parsed.text !== "string") {
      return null;
    }
    return parsed;
  } catch {
    return null;
  }
}

export default function OptimizeWindow(): JSX.Element {
  const [sourceText, setSourceText] = useState("");
  const [title, setTitle] = useState("优化");
  const [customPrompt, setCustomPrompt] = useState<string | undefined>(undefined);
  const [customModel, setCustomModel] = useState<string | undefined>(undefined);
  const [requestId, setRequestId] = useState(0);
  const fw = useFeatureWindow();
  const ai = useStreamingAI("优化失败");

  useEffect(() => {
    const applyPayload = (payload: ChangeTextPayload): void => {
      if (payload.target && payload.target !== "optimize") {
        return;
      }

      if (typeof payload.title === "string" && payload.title.trim()) {
        setTitle(payload.title.trim());
      } else {
        setTitle("优化");
      }

      setCustomPrompt(payload.customPrompt?.trim() || undefined);
      setCustomModel(payload.customModel?.trim() || undefined);

      if (typeof payload.text === "string") {
        setSourceText(payload.text);
        setRequestId(payload.requestId ?? Date.now());
      }
    };

    const pending = safeParsePayload(window.localStorage.getItem(LAST_OPTIMIZE_REQUEST_KEY));
    if (pending) {
      applyPayload(pending);
      window.localStorage.removeItem(LAST_OPTIMIZE_REQUEST_KEY);
    }

    let unlisten: (() => void) | undefined;

    listen<ChangeTextPayload>("change-text", (event) => {
      applyPayload(event.payload);
      if (event.payload.target === "optimize") {
        window.localStorage.removeItem(LAST_OPTIMIZE_REQUEST_KEY);
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

    ai.startStream("optimize", sourceText, {
      customPrompt,
      customModel
    });
  }, [customModel, customPrompt, requestId, sourceText]);

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
