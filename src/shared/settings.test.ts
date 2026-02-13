import { describe, expect, it } from "vitest";
import { defaultSettings, validateSettings } from "./settings";

describe("settings schema", () => {
  it("has api section first and enabled action defaults", () => {
    const settings = defaultSettings();
    expect(settings.api.model).not.toBe("");
    expect(settings.toolbar.actions[0].id).toBe("translate");
  });

  it("includes theme mode in toolbar defaults", () => {
    const settings = defaultSettings();
    expect(settings.toolbar.themeMode).toBe("dark");
    expect(settings.toolbar.triggerHotkey).toBe("Ctrl+Shift+Space");
    expect(settings.general.language).toBe("zh-CN");
  });

  it("rejects invalid baseUrl", () => {
    expect(() => validateSettings({ api: { baseUrl: "abc" } as any })).toThrow();
  });
});
