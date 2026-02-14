import { useCallback, useRef, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import { listen, type UnlistenFn } from "@tauri-apps/api/event";

interface StreamChunkPayload {
  streamId: string;
  chunk: string;
}

interface StreamDonePayload {
  streamId: string;
  fullText: string;
  elapsedMs: number;
}

interface StreamErrorPayload {
  streamId: string;
  error: string;
}

export interface StreamingAIState {
  resultText: string;
  loading: boolean;
  streaming: boolean;
  errorText: string | undefined;
}

export interface StreamingAIActions {
  startStream: (
    taskKind: string,
    text: string,
    options?: Record<string, unknown>
  ) => void;
  reset: () => void;
}

/**
 * Hook to invoke `stream_process_text` and accumulate SSE chunks into
 * resultText. Returns state and an action to kick off a new stream.
 *
 * The `errorLabel` parameter is a Chinese prefix like "翻译失败" / "总结失败".
 */
export function useStreamingAI(errorLabel: string): StreamingAIState & StreamingAIActions {
  const [resultText, setResultText] = useState("");
  const [loading, setLoading] = useState(false);
  const [streaming, setStreaming] = useState(false);
  const [errorText, setErrorText] = useState<string | undefined>();

  // Track the current stream_id so stale events are ignored.
  const activeStreamId = useRef<string | null>(null);
  const unlisteners = useRef<UnlistenFn[]>([]);

  const cleanup = useCallback(() => {
    for (const fn of unlisteners.current) {
      fn();
    }
    unlisteners.current = [];
  }, []);

  const acceptStreamEvent = useCallback((incomingStreamId: string): boolean => {
    if (!activeStreamId.current) {
      activeStreamId.current = incomingStreamId;
      return true;
    }

    return activeStreamId.current === incomingStreamId;
  }, []);

  const reset = useCallback(() => {
    cleanup();
    activeStreamId.current = null;
    setResultText("");
    setLoading(false);
    setStreaming(false);
    setErrorText(undefined);
  }, [cleanup]);

  const startStream = useCallback(
    (taskKind: string, text: string, options?: Record<string, unknown>) => {
      // Abort any previous listeners
      cleanup();
      activeStreamId.current = null;
      setResultText("");
      setErrorText(undefined);
      setLoading(true);
      setStreaming(false);

      const run = async (): Promise<void> => {
        try {
          // Set up listeners BEFORE invoking the command so we never miss
          // early chunks.
          const [chunkUn, doneUn, errorUn] = await Promise.all([
            listen<StreamChunkPayload>("stream-chunk", (event) => {
              if (!acceptStreamEvent(event.payload.streamId)) {
                return;
              }
              setStreaming(true);
              setLoading(false);
              setResultText((prev) => prev + event.payload.chunk);
            }),
            listen<StreamDonePayload>("stream-done", (event) => {
              if (!acceptStreamEvent(event.payload.streamId)) {
                return;
              }

              setResultText(event.payload.fullText);
              setStreaming(false);
              setLoading(false);
            }),
            listen<StreamErrorPayload>("stream-error", (event) => {
              if (!acceptStreamEvent(event.payload.streamId)) {
                return;
              }

              setErrorText(`${errorLabel}：${event.payload.error}`);
              setStreaming(false);
              setLoading(false);
            })
          ]);

          unlisteners.current = [chunkUn, doneUn, errorUn];

          const streamId = await invoke<string>("stream_process_text", {
            taskKind,
            text,
            options: options ?? null,
          });

          if (!activeStreamId.current) {
            activeStreamId.current = streamId;
          }
        } catch (error) {
          const message = error instanceof Error ? error.message : String(error);
          setErrorText(`${errorLabel}：${message}`);
          setLoading(false);
          setStreaming(false);
        }
      };

      void run();
    },
    [acceptStreamEvent, cleanup, errorLabel]
  );

  return { resultText, loading, streaming, errorText, startStream, reset };
}
