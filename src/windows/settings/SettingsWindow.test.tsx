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
    mocks.invokeMock.mockImplementation(async (command: string) => {
      if (command === "get_settings") {
        return defaultSettings();
      }

      if (command === "test_api_connection") {
        return {
          model: "gpt-4o-mini",
          message: "ok",
          elapsedMs: 42
        };
      }

      return defaultSettings();
    });
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

  it("shows theme mode selector inside 通用设置 section", async () => {
    await renderWindow();

    fireEvent.click(screen.getByRole("tab", { name: "通用设置" }));

    expect(screen.getByLabelText("界面主题")).toBeInTheDocument();
  });

  it("allows temporarily invalid api fields while editing", async () => {
    await renderWindow();

    const baseUrlInput = screen.getByLabelText("Base URL");
    const modelInput = screen.getByLabelText("翻译模型");

    fireEvent.change(baseUrlInput, { target: { value: "" } });
    fireEvent.change(modelInput, { target: { value: "" } });

    expect(baseUrlInput).toHaveValue("");
    expect(modelInput).toHaveValue("");
  });

  it("supports testing api connectivity from api section", async () => {
    await renderWindow();

    fireEvent.click(screen.getByRole("button", { name: "测试 API" }));

    expect(mocks.invokeMock).toHaveBeenCalledWith(
      "test_api_connection",
      expect.objectContaining({
        api: expect.objectContaining({
          baseUrl: "https://api.openai.com/v1"
        })
      })
    );

    expect(await screen.findByText(/测试通过/i)).toBeInTheDocument();
  });

  it("shows optimize model field in api section", async () => {
    await renderWindow();
    expect(screen.getByLabelText("优化模型")).toBeInTheDocument();
  });
});
