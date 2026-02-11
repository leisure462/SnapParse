import { fireEvent, render, screen } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import ActionBarWindow from "./ActionBarWindow";

const invokeMock = vi.fn(async () => undefined);
const emitMock = vi.fn(async () => undefined);

vi.mock("@tauri-apps/api/core", () => ({
  invoke: (...args: unknown[]) => invokeMock(...args)
}));

vi.mock("@tauri-apps/api/event", () => ({
  emit: (...args: unknown[]) => emitMock(...args),
  listen: vi.fn(async () => () => undefined)
}));

describe("ActionBarWindow", () => {
  beforeEach(() => {
    invokeMock.mockClear();
    emitMock.mockClear();
    document.documentElement.removeAttribute("data-theme");
    window.localStorage.clear();
  });

  it("renders default five actions", () => {
    render(<ActionBarWindow />);
    expect(screen.getByText("翻译")).toBeInTheDocument();
    expect(screen.getByText("解释")).toBeInTheDocument();
    expect(screen.getByText("总结")).toBeInTheDocument();
    expect(screen.getByText("搜索")).toBeInTheDocument();
    expect(screen.getByText("复制")).toBeInTheDocument();
  });

  it("renders toolbar theme toggle and flips mode", () => {
    render(<ActionBarWindow />);
    const toggle = screen.getByRole("switch", { name: "明暗切换" });
    expect(toggle).toBeInTheDocument();

    const before = toggle.getAttribute("aria-checked");
    fireEvent.click(toggle);
    const after = toggle.getAttribute("aria-checked");

    expect(before).not.toBe(after);
  });
});
