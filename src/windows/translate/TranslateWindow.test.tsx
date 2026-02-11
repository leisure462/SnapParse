import { act, render, screen, waitFor } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import TranslateWindow from "./TranslateWindow";

type EventHandler = (event: { payload: { text: string } }) => void;

const listeners = new Map<string, EventHandler>();
const invokeMock = vi.fn();

vi.mock("@tauri-apps/api/core", () => ({
  invoke: (...args: unknown[]) => invokeMock(...args)
}));

vi.mock("@tauri-apps/api/event", () => ({
  listen: vi.fn(async (name: string, handler: EventHandler) => {
    listeners.set(name, handler);
    return () => listeners.delete(name);
  })
}));

vi.mock("@tauri-apps/api/window", () => ({
  getCurrentWindow: () => ({
    minimize: vi.fn(async () => undefined),
    hide: vi.fn(async () => undefined)
  })
}));

describe("TranslateWindow", () => {
  beforeEach(() => {
    listeners.clear();
    invokeMock.mockReset();
  });

  it("shows original text after change-text event", async () => {
    invokeMock.mockResolvedValue({
      taskKind: "translate",
      sourceText: "hello",
      resultText: "你好",
      usedModel: "gpt-4o-mini",
      elapsedMs: 120
    });

    render(<TranslateWindow />);

    await act(async () => {
      listeners.get("change-text")?.({ payload: { text: "hello" } });
    });

    expect(await screen.findByDisplayValue("hello")).toBeInTheDocument();
  });

  it("shows loading then translated result", async () => {
    let resolver: ((value: unknown) => void) | undefined;
    invokeMock.mockImplementation(
      () =>
        new Promise((resolve) => {
          resolver = resolve;
        })
    );

    render(<TranslateWindow />);

    await act(async () => {
      listeners.get("change-text")?.({ payload: { text: "test input" } });
    });

    expect(screen.getByText("处理中...")).toBeInTheDocument();

    await act(async () => {
      resolver?.({
        taskKind: "translate",
        sourceText: "test input",
        resultText: "translated output",
        usedModel: "gpt-4o-mini",
        elapsedMs: 190
      });
    });

    await waitFor(() => {
      expect(screen.getByText("translated output")).toBeInTheDocument();
    });
  });
});
