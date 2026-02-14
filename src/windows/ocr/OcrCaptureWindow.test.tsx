import { act, fireEvent, render, screen, waitFor } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import OcrCaptureWindow from "./OcrCaptureWindow";
import { defaultSettings } from "../../shared/settings";

type CaptureEventHandler = (event: { payload: any }) => void;

const mocks = vi.hoisted(() => ({
  invokeMock: vi.fn(),
  listenHandlers: new Map<string, CaptureEventHandler>(),
  hideMock: vi.fn()
}));

vi.mock("@tauri-apps/api/core", () => ({
  invoke: mocks.invokeMock
}));

vi.mock("@tauri-apps/api/event", () => ({
  listen: vi.fn(async (eventName: string, handler: CaptureEventHandler) => {
    mocks.listenHandlers.set(eventName, handler);
    return () => {
      mocks.listenHandlers.delete(eventName);
    };
  })
}));

vi.mock("@tauri-apps/api/window", () => ({
  getCurrentWindow: vi.fn(() => ({
    hide: mocks.hideMock
  }))
}));

describe("OcrCaptureWindow", () => {
  beforeEach(() => {
    mocks.invokeMock.mockReset();
    mocks.hideMock.mockReset();
    mocks.listenHandlers.clear();
    mocks.invokeMock.mockImplementation(async (command: string) => {
      if (command === "get_settings") {
        return defaultSettings();
      }

      if (command === "capture_screenshot_preview") {
        return {
          dataUrl: "data:image/png;base64,ZmFrZQ==",
          logicalRect: {
            x: 10,
            y: 10,
            width: 110,
            height: 80
          }
        };
      }

      return undefined;
    });
  });

  async function dragSelect(x1: number, y1: number, x2: number, y2: number): Promise<void> {
    const shell = screen.getByRole("application", { name: "截屏窗口" });

    await act(async () => {
      fireEvent.mouseDown(shell, { button: 0, clientX: x1, clientY: y1 });
    });

    await act(async () => {
      fireEvent.mouseMove(shell, { clientX: x2, clientY: y2 });
    });

    await act(async () => {
      fireEvent.mouseUp(shell, { button: 0, clientX: x2, clientY: y2 });
    });
  }

  it("captures region first then can trigger OCR action", async () => {
    render(<OcrCaptureWindow />);

    await dragSelect(10, 10, 120, 90);

    await waitFor(() => {
      expect(mocks.invokeMock).toHaveBeenCalledWith(
        "capture_screenshot_preview",
        expect.objectContaining({
          request: expect.objectContaining({
            mode: "region",
            region: expect.objectContaining({
              x: 10,
              y: 10,
              width: 110,
              height: 80
            })
          })
        })
      );
    });

    fireEvent.click(screen.getByRole("button", { name: "OCR识别" }));

    await waitFor(() => {
      expect(mocks.invokeMock).toHaveBeenCalledWith(
        "run_ocr_capture",
        expect.objectContaining({
          region: expect.objectContaining({
            x: 10,
            y: 10,
            width: 110,
            height: 80
          })
        })
      );
    });
  });
});
