import { act, fireEvent, render, screen } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import SettingsWindow from "./SettingsWindow";
import { defaultSettings } from "../../shared/settings";

const mocks = vi.hoisted(() => ({
  invokeMock: vi.fn()
}));

vi.mock("@tauri-apps/api/core", () => ({
  invoke: mocks.invokeMock
}));

describe("SettingsWindow", () => {
  beforeEach(() => {
    mocks.invokeMock.mockReset();
    mocks.invokeMock.mockResolvedValue(defaultSettings());
  });

  async function renderWindow(): Promise<void> {
    await act(async () => {
      render(<SettingsWindow />);
      await Promise.resolve();
    });
  }

  it("opens API settings as default first section", async () => {
    await renderWindow();
    expect(screen.getByRole("tab", { name: "API配置" })).toHaveAttribute("aria-selected", "true");
  });

  it("shows theme controls inside 工具栏 section", async () => {
    await renderWindow();

    fireEvent.click(screen.getByRole("tab", { name: "工具栏" }));

    expect(screen.getByLabelText("工具栏明暗切换")).toBeInTheDocument();
  });
});
