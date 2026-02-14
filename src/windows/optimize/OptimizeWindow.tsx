import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import { getCurrentWindow } from "@tauri-apps/api/window";
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

function normalizePayload(payload: ChangeTextPayload): ChangeTextPayload | null {
  if (payload.target && payload.target !== "optimize") {
    return null;
  }

  if (typeof payload.text !== "string" || !payload.text.trim()) {
    return null;
  }

  return {
    ...payload,
    text: payload.text.trim(),
    title: payload.title?.trim(),
    customPrompt: payload.customPrompt?.trim(),
    customModel: payload.customModel?.trim()
  };
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
      const normalized = normalizePayload(payload);
      if (!normalized) {
        return;
      }

      if (normalized.title) {
        setTitle(normalized.title);
      } else {
        setTitle("优化");
      }

      setCustomPrompt(normalized.customPrompt || undefined);
      setCustomModel(normalized.customModel || undefined);
      setSourceText(normalized.text);
      setRequestId(normalized.requestId ?? Date.now());
    };

    const consumePendingRequest = async (): Promise<void> => {
      try {
        const pending = await invoke<ChangeTextPayload | null>("take_pending_optimize_request");
        if (pending) {
          applyPayload(pending);
        }
      } catch {
        // noop
      }
    };

    void consumePendingRequest();

    let unlistenText: (() => void) | undefined;
    let unlistenPending: (() => void) | undefined;
    let unlistenFocus: (() => void) | undefined;

    listen<ChangeTextPayload>("change-text", (event) => {
      applyPayload(event.payload);
    }).then((cleanup) => {
      unlistenText = cleanup;
    });

    listen<{ requestId?: number }>("optimize-pending-updated", () => {
      void consumePendingRequest();
    }).then((cleanup) => {
      unlistenPending = cleanup;
    });

    getCurrentWindow().onFocusChanged(({ payload: focused }) => {
      if (focused) {
        void consumePendingRequest();
      }
    }).then((cleanup) => {
      unlistenFocus = cleanup;
    }).catch(() => {});

    return () => {
      unlistenText?.();
      unlistenPending?.();
      unlistenFocus?.();
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
