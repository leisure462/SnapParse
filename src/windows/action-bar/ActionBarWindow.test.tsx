import { act, fireEvent, render, screen, waitFor } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import ActionBarWindow from "./ActionBarWindow";
import { defaultSettings } from "../../shared/settings";

type EventHandler = (event: { payload: { text: string } }) => void;

const mocks = vi.hoisted(() => ({
  listeners: new Map<string, EventHandler>(),
  invokeMock: vi.fn(),
  emitMock: vi.fn()
}));

const listeners = mocks.listeners;

vi.mock("@tauri-apps/api/core", () => ({
  invoke: mocks.invokeMock
}));

vi.mock("@tauri-apps/api/event", () => ({
  emit: mocks.emitMock,
  listen: vi.fn(async (name: string, handler: EventHandler) => {
    listeners.set(name, handler);
    return () => listeners.delete(name);
  })
}));

describe("ActionBarWindow", () => {
  beforeEach(() => {
    listeners.clear();
    mocks.invokeMock.mockReset();
    mocks.invokeMock.mockImplementation(async (command: string) => {
      if (command === "get_settings") {
        return defaultSettings();
      }

      return undefined;
    });
    mocks.emitMock.mockReset();
    mocks.emitMock.mockResolvedValue(undefined);
    document.documentElement.removeAttribute("data-theme");
    window.localStorage.clear();
  });

  async function renderWindow(): Promise<void> {
    await act(async () => {
      render(<ActionBarWindow />);
      await Promise.resolve();
    });
  }

  it("renders default five actions", async () => {
    await renderWindow();
    expect(screen.getByText("翻译")).toBeInTheDocument();
    expect(screen.getByText("解释")).toBeInTheDocument();
    expect(screen.getByText("总结")).toBeInTheDocument();
    expect(screen.getByText("搜索")).toBeInTheDocument();
    expect(screen.getByText("复制")).toBeInTheDocument();
  });

  it("does not render toolbar theme toggle in action bar", async () => {
    await renderWindow();
    expect(screen.queryByRole("switch", { name: "明暗切换" })).not.toBeInTheDocument();
  });

  it("closes action bar before opening translate window", async () => {
    await renderWindow();

    await act(async () => {
      listeners.get("selection-text-changed")?.({ payload: { text: "hello world" } });
    });

    fireEvent.click(screen.getByRole("button", { name: "翻译" }));

    await waitFor(() => {
      expect(mocks.emitMock).toHaveBeenCalledWith("change-text", {
        text: "hello world",
        source: "action-bar"
      });
    });

    const commandCalls = mocks.invokeMock.mock.calls.map((args) => args[0]);
    const closeIndex = commandCalls.indexOf("close_window");
    const openIndex = commandCalls.indexOf("open_window");
    const moveIndex = commandCalls.indexOf("move_window");

    expect(closeIndex).toBeGreaterThanOrEqual(0);
    expect(openIndex).toBeGreaterThanOrEqual(0);
    expect(moveIndex).toBeGreaterThanOrEqual(0);
    expect(closeIndex).toBeLessThan(openIndex);
    expect(window.localStorage.getItem("snapparse:selected-text")).toBe("hello world");
  });
});
