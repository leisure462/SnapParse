import { useEffect, useMemo, useState } from "react";
import ReactMarkdown from "react-markdown";
import remarkGfm from "remark-gfm";

interface ResultPanelProps {
  originalText: string;
  resultText: string;
  loading: boolean;
  loadingLabel?: string;
  /** When true, resultText is still being appended via streaming. */
  streaming?: boolean;
  error?: string;
}

async function copyToClipboard(value: string): Promise<void> {
  if (!value.trim()) {
    return;
  }

  try {
    await navigator.clipboard.writeText(value);
  } catch {
    const fallback = document.createElement("textarea");
    fallback.value = value;
    fallback.style.position = "fixed";
    fallback.style.opacity = "0";
    document.body.append(fallback);
    fallback.select();
    document.execCommand("copy");
    fallback.remove();
  }
}

export default function ResultPanel(props: ResultPanelProps): JSX.Element {
  const [showOriginal, setShowOriginal] = useState(false);
  const [copied, setCopied] = useState(false);

  const hasResult = props.resultText.trim().length > 0;
  const showLoadingDots = props.loading && !props.streaming && !hasResult;
  const showContent = hasResult || props.streaming;
  const canCopy = !props.error && hasResult;

  useEffect(() => {
    if (!copied) {
      return;
    }

    const timer = window.setTimeout(() => {
      setCopied(false);
    }, 1200);

    return () => {
      window.clearTimeout(timer);
    };
  }, [copied]);

  const renderedMarkdown = useMemo(() => {
    return <ReactMarkdown remarkPlugins={[remarkGfm]}>{props.resultText}</ReactMarkdown>;
  }, [props.resultText]);

  return (
    <section className="md2-result-panel" aria-live="polite">
      {canCopy ? (
        <button
          type="button"
          className={`md2-result-copy-btn ${copied ? "is-copied" : ""}`}
          aria-label={copied ? "已复制" : "复制结果"}
          title={copied ? "已复制" : "复制结果"}
          onClick={() => {
            void copyToClipboard(props.resultText).then(() => {
              setCopied(true);
            });
          }}
        >
          <svg
            viewBox="0 0 24 24"
            width="12"
            height="12"
            fill="none"
            stroke="currentColor"
            strokeWidth="2"
            strokeLinecap="round"
            strokeLinejoin="round"
            aria-hidden="true"
          >
            <rect x="8" y="8" width="11" height="12" rx="2" />
            <path d="M5 15V6a2 2 0 0 1 2-2h8" />
          </svg>
        </button>
      ) : null}

      {showLoadingDots ? (
        <div className="md2-result-loading">
          <div className="md2-loading-dots">
            <span /><span /><span />
          </div>
          <span className="md2-loading-label">{props.loadingLabel ?? "处理中..."}</span>
        </div>
      ) : null}

      {!props.loading && props.error ? (
        <div className="md2-result-error">{props.error}</div>
      ) : null}

      {showContent && !props.error ? (
        <div className="md2-result-content">
          {props.streaming ? (
            <div className="md2-result-streaming-text">
              {props.resultText}
              <span className="md2-streaming-cursor" />
            </div>
          ) : (
            renderedMarkdown
          )}
        </div>
      ) : null}

      {!props.loading && !hasResult && !props.error && !props.streaming ? (
        <div className="md2-result-content">
          <p>暂无结果</p>
        </div>
      ) : null}

      {props.originalText.trim() ? (
        <div className="md2-original-section">
          <button
            type="button"
            className="md2-original-toggle"
            onClick={() => {
              setShowOriginal((prev) => !prev);
            }}
          >
            <svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"
              className={showOriginal ? "md2-chevron-open" : "md2-chevron-closed"}>
              <polyline points="6 9 12 15 18 9" />
            </svg>
            <span>{showOriginal ? "收起原文" : "展开原文"}</span>
          </button>
          {showOriginal ? (
            <div className="md2-original-text">{props.originalText}</div>
          ) : null}
        </div>
      ) : null}
    </section>
  );
}
