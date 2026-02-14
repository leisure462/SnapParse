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

  it("opens 通用设置 as default first section", async () => {
    await renderWindow();
    expect(screen.getByRole("tab", { name: "通用设置" })).toHaveAttribute("aria-selected", "true");
  });

  it("shows theme mode selector inside 通用设置 section", async () => {
    await renderWindow();

    fireEvent.click(screen.getByRole("tab", { name: "通用设置" }));

    expect(screen.getByLabelText("界面主题")).toBeInTheDocument();
  });

  it("allows temporarily invalid api fields while editing", async () => {
    await renderWindow();
    fireEvent.click(screen.getByRole("tab", { name: "API配置" }));

    const baseUrlInput = screen.getByLabelText("Base URL");
    const modelInput = screen.getByLabelText("翻译模型");

    fireEvent.change(baseUrlInput, { target: { value: "" } });
    fireEvent.change(modelInput, { target: { value: "" } });

    expect(baseUrlInput).toHaveValue("");
    expect(modelInput).toHaveValue("");
  });

  it("supports testing api connectivity from api section", async () => {
    await renderWindow();
    fireEvent.click(screen.getByRole("tab", { name: "API配置" }));

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
    fireEvent.click(screen.getByRole("tab", { name: "API配置" }));
    expect(screen.getByLabelText("优化模型")).toBeInTheDocument();
  });

  it("shows ocr section fields", async () => {
    await renderWindow();
    fireEvent.click(screen.getByRole("tab", { name: "OCR配置" }));
    expect(screen.getByLabelText("OCR 服务类型")).toBeInTheDocument();
    expect(screen.getByLabelText("自动执行功能")).toBeInTheDocument();
    expect(screen.queryByLabelText("OCR 快捷键")).not.toBeInTheDocument();
  });

  it("shows screenshot section", async () => {
    await renderWindow();
    fireEvent.click(screen.getByRole("tab", { name: "截屏设置" }));
    expect(screen.getByText("默认截屏模式")).toBeInTheDocument();
    expect(screen.getByLabelText("显示左下角快捷键提示")).toBeInTheDocument();
  });

  it("shows OCR hotkey input in 快捷键设置 section", async () => {
    await renderWindow();
    fireEvent.click(screen.getByRole("tab", { name: "快捷键设置" }));
    expect(screen.getByLabelText("截屏快捷键（区域）")).toBeInTheDocument();
    expect(screen.getByLabelText("OCR 快捷键")).toBeInTheDocument();
    expect(screen.getByLabelText("全屏模式快捷键")).toBeInTheDocument();
  });
});
