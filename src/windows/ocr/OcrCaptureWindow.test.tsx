import { act, fireEvent, render, screen, waitFor } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import OcrCaptureWindow from "./OcrCaptureWindow";

type CaptureEventHandler = () => void;

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
    mocks.invokeMock.mockResolvedValue(undefined);
  });

  async function dragSelect(x1: number, y1: number, x2: number, y2: number): Promise<void> {
    const shell = screen.getByRole("application", { name: "OCR截图窗口" });

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

  it("allows running capture repeatedly without stale locked state", async () => {
    render(<OcrCaptureWindow />);

    await dragSelect(10, 10, 120, 90);
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

    await dragSelect(30, 40, 180, 150);
    await waitFor(() => {
      const runCalls = mocks.invokeMock.mock.calls.filter((item) => item[0] === "run_ocr_capture");
      expect(runCalls.length).toBe(2);
    });
  });
});
