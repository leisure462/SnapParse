import { describe, expect, it } from "vitest";
import { defaultSettings, validateSettings } from "./settings";

describe("settings schema", () => {
  it("has api section first and enabled action defaults", () => {
    const settings = defaultSettings();
    expect(settings.api.model).not.toBe("");
    expect(settings.toolbar.actions[0].id).toBe("translate");
  });

  it("includes theme controls in toolbar defaults", () => {
    const settings = defaultSettings();
    expect(settings.toolbar.themeMode).toBe("dark");
    expect(settings.toolbar.showThemeToggleInToolbar).toBe(true);
  });

  it("rejects invalid baseUrl", () => {
    expect(() => validateSettings({ api: { baseUrl: "abc" } as any })).toThrow();
  });
});
