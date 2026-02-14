import { describe, expect, it } from "vitest";
import { defaultSettings, MAX_CUSTOM_ACTION_COUNT, MAX_CUSTOM_ACTION_NAME_LENGTH, validateSettings } from "./settings";

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
    expect(settings.api.featureModels.optimize).toBe("gpt-4o-mini");
    expect(settings.ocr.captureHotkey).toBe("Ctrl+Shift+X");
    expect(settings.ocr.quickOcrHotkey).toBe("Alt+S");
    expect(settings.ocr.modeHotkeys.fullscreen).toBe("Ctrl+A");
    expect(settings.ocr.modeHotkeys.window).toBe("Ctrl+M");
    expect(settings.ocr.captureDefaultMode).toBe("region");
    expect(settings.ocr.postActionId).toBe("translate");
    expect(settings.features.enabledActions).toContain("optimize");
  });

  it("rejects invalid baseUrl", () => {
    expect(() => validateSettings({ api: { baseUrl: "abc" } as any })).toThrow();
  });

  it("allows empty OCR prompt", () => {
    const settings = validateSettings({
      ocr: {
        prompt: ""
      } as any
    });

    expect(settings.ocr.prompt).toBe("");
  });

  it("rejects duplicate custom action ids", () => {
    expect(() =>
      validateSettings({
        features: {
          customActions: [
            { id: "c1", name: "a", icon: "sparkles", prompt: "{{text}}", model: "", enabled: true, order: 0 },
            { id: "c1", name: "b", icon: "sparkles", prompt: "{{text}}", model: "", enabled: true, order: 1 }
          ]
        } as any
      })
    ).toThrow();
  });

  it("fills missing custom action model with empty string", () => {
    const settings = validateSettings({
      features: {
        customActions: [
          { id: "c1", name: "a", icon: "sparkles", prompt: "{{text}}", enabled: true, order: 0 }
        ]
      } as any
    });

    expect(settings.features.customActions[0].model).toBe("");
  });

  it("caps custom action count to configured max", () => {
    const settings = validateSettings({
      features: {
        customActions: Array.from({ length: MAX_CUSTOM_ACTION_COUNT + 2 }, (_, index) => ({
          id: `c-${index}`,
          name: `a${index}`,
          icon: "bot",
          prompt: "{{text}}",
          enabled: true,
          order: index
        }))
      } as any
    });

    expect(settings.features.customActions).toHaveLength(MAX_CUSTOM_ACTION_COUNT);
  });

  it("clamps custom action name length", () => {
    const settings = validateSettings({
      features: {
        customActions: [
          {
            id: "c1",
            name: "123456789abcdef",
            icon: "bot",
            prompt: "{{text}}",
            enabled: true,
            order: 0
          }
        ]
      } as any
    });

    expect(Array.from(settings.features.customActions[0].name).length).toBeLessThanOrEqual(MAX_CUSTOM_ACTION_NAME_LENGTH);
  });
});
