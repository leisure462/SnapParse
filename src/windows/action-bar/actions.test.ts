import { describe, expect, it } from "vitest";
import { defaultSettings } from "../../shared/settings";
import { resolveActionBarActions } from "./actions";

describe("resolveActionBarActions", () => {
  it("includes built-in optimize action by default", () => {
    const settings = defaultSettings();
    const actions = resolveActionBarActions(settings);
    expect(actions.map((item) => item.id)).toContain("optimize");
  });

  it("hides disabled built-in actions", () => {
    const settings = defaultSettings();
    settings.toolbar.actions = settings.toolbar.actions.map((item) =>
      item.id === "copy"
        ? {
            ...item,
            enabled: false
          }
        : item
    );

    const actions = resolveActionBarActions(settings);
    expect(actions.map((item) => item.id)).not.toContain("copy");
  });

  it("appends enabled custom actions regardless of global toggle", () => {
    const settings = defaultSettings();
    settings.features.customActionsEnabled = false;
    settings.features.customActions = [
      {
        id: "custom-tone",
        name: "商务润色",
        icon: "briefcase",
        prompt: "请把下面内容改写为商务语气：\n{{text}}",
        model: "gpt-4o-mini",
        enabled: true,
        order: 0
      }
    ];

    const actions = resolveActionBarActions(settings);
    expect(actions.map((item) => item.id)).toContain("custom-tone");
    expect(actions.find((item) => item.id === "custom-tone")?.commandWindow).toBe("optimize");
    expect(actions.find((item) => item.id === "custom-tone")?.model).toBe("gpt-4o-mini");
  });
});
