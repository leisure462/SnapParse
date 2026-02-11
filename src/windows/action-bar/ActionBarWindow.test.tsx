import { act, render, screen } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import ActionBarWindow from "./ActionBarWindow";
import { defaultSettings } from "../../shared/settings";

const mocks = vi.hoisted(() => ({
  invokeMock: vi.fn(),
  emitMock: vi.fn()
}));

vi.mock("@tauri-apps/api/core", () => ({
  invoke: mocks.invokeMock
}));

vi.mock("@tauri-apps/api/event", () => ({
  emit: mocks.emitMock,
  listen: vi.fn(async () => () => undefined)
}));

describe("ActionBarWindow", () => {
  beforeEach(() => {
    mocks.invokeMock.mockReset();
    mocks.invokeMock.mockResolvedValue(defaultSettings());
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
});
