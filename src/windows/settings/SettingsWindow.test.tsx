import { fireEvent, render, screen } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import SettingsWindow from "./SettingsWindow";
import { defaultSettings } from "../../shared/settings";

const invokeMock = vi.fn();

vi.mock("@tauri-apps/api/core", () => ({
  invoke: (...args: unknown[]) => invokeMock(...args)
}));

describe("SettingsWindow", () => {
  beforeEach(() => {
    invokeMock.mockReset();
    invokeMock.mockResolvedValue(defaultSettings());
  });

  it("opens API settings as default first section", () => {
    render(<SettingsWindow />);
    expect(screen.getByRole("tab", { name: "API配置" })).toHaveAttribute("aria-selected", "true");
  });

  it("shows theme controls inside 工具栏 section", () => {
    render(<SettingsWindow />);

    fireEvent.click(screen.getByRole("tab", { name: "工具栏" }));

    expect(screen.getByLabelText("工具栏明暗切换")).toBeInTheDocument();
  });
});
