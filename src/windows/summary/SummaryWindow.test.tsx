import { act, render, screen, waitFor } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import SummaryWindow from "./SummaryWindow";

type EventHandler = (event: { payload: { text: string } }) => void;

const mocks = vi.hoisted(() => ({
  listeners: new Map<string, EventHandler>(),
  invokeMock: vi.fn()
}));

const listeners = mocks.listeners;

vi.mock("@tauri-apps/api/core", () => ({
  invoke: mocks.invokeMock
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

describe("SummaryWindow", () => {
  beforeEach(() => {
    listeners.clear();
    mocks.invokeMock.mockReset();
  });

  it("shows original text after change-text event", async () => {
    mocks.invokeMock.mockResolvedValue({
      taskKind: "summarize",
      sourceText: "origin text",
      resultText: "summary text",
      usedModel: "gpt-4o-mini",
      elapsedMs: 120
    });

    render(<SummaryWindow />);

    await act(async () => {
      listeners.get("change-text")?.({ payload: { text: "origin text" } });
    });

    expect(await screen.findByDisplayValue("origin text")).toBeInTheDocument();
  });

  it("shows loading then result for summarize request", async () => {
    let resolver: ((value: unknown) => void) | undefined;
    mocks.invokeMock.mockImplementation(
      () =>
        new Promise((resolve) => {
          resolver = resolve;
        })
    );

    render(<SummaryWindow />);

    await act(async () => {
      listeners.get("change-text")?.({ payload: { text: "long article" } });
    });

    expect(screen.getByText("处理中...")).toBeInTheDocument();

    await act(async () => {
      resolver?.({
        taskKind: "summarize",
        sourceText: "long article",
        resultText: "short summary",
        usedModel: "gpt-4o-mini",
        elapsedMs: 210
      });
    });

    await waitFor(() => {
      expect(screen.getByText("short summary")).toBeInTheDocument();
    });
  });
});
