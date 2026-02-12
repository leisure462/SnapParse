import { useMemo, useState } from "react";

interface ResultPanelProps {
  originalText: string;
  resultText: string;
  loading: boolean;
  /** When true, resultText is still being appended via streaming. */
  streaming?: boolean;
  error?: string;
}

function toParagraphs(value: string): string[] {
  return value
    .split(/\n{2,}|\r\n\r\n/)
    .map((item) => item.trim())
    .filter(Boolean);
}

export default function ResultPanel(props: ResultPanelProps): JSX.Element {
  const [showOriginal, setShowOriginal] = useState(false);

  const paragraphs = useMemo(() => toParagraphs(props.resultText), [props.resultText]);

  const hasResult = props.resultText.trim().length > 0;
  const showLoadingDots = props.loading && !props.streaming && !hasResult;
  const showContent = hasResult || props.streaming;

  return (
    <section className="md2-result-panel" aria-live="polite">
      {showLoadingDots ? (
        <div className="md2-result-loading">
          <div className="md2-loading-dots">
            <span /><span /><span />
          </div>
          <span className="md2-loading-label">处理中...</span>
        </div>
      ) : null}

      {!props.loading && props.error ? (
        <div className="md2-result-error">{props.error}</div>
      ) : null}

      {showContent && !props.error ? (
        <div className="md2-result-content">
          {(paragraphs.length > 0 ? paragraphs : [props.resultText || ""]).map((item, index) => (
            <p key={`${index}`}>{item}</p>
          ))}
          {props.streaming ? <span className="md2-streaming-cursor" /> : null}
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
