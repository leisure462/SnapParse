import { useMemo, useState } from "react";

interface ResultPanelProps {
  originalText: string;
  resultText: string;
  loading: boolean;
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

  return (
    <section className="md2-result-panel" aria-live="polite">
      <div className="md2-result-toolbar">
        <button
          type="button"
          className="md2-ghost-btn"
          onClick={() => {
            setShowOriginal((prev) => !prev);
          }}
        >
          {showOriginal ? "隐藏原文" : "显示原文"}
        </button>
      </div>

      {showOriginal ? <article className="md2-original-text">{props.originalText || "暂无原文"}</article> : null}

      {props.loading ? <p className="md2-status-text">处理中...</p> : null}
      {!props.loading && props.error ? <p className="md2-status-text error">{props.error}</p> : null}

      {!props.loading && !props.error ? (
        <article className="md2-result-text">
          {(paragraphs.length > 0 ? paragraphs : [props.resultText || "暂无结果"]).map((item, index) => (
            <p key={`${item}-${index}`}>{item}</p>
          ))}
        </article>
      ) : null}
    </section>
  );
}
