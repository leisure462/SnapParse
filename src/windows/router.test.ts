import { describe, expect, it } from "vitest";
import { resolveWindowFromLocation, resolveWindowFromRuntime, resolveWindowRoute } from "./router";

describe("window router", () => {
  it("resolves summary window route", () => {
    expect(resolveWindowRoute("summary")).toBe("/windows/summary");
  });

  it("resolves optimize window route", () => {
    expect(resolveWindowRoute("optimize")).toBe("/windows/optimize");
  });

  it("resolves location query to window key", () => {
    expect(resolveWindowFromLocation("?window=settings")).toBe("settings");
  });

  it("falls back to main for unknown key", () => {
    expect(resolveWindowFromLocation("?window=unknown")).toBe("main");
  });

  it("reads runtime injected window key", () => {
    (window as Window & { __SNAPPARSE_WINDOW_KIND?: string }).__SNAPPARSE_WINDOW_KIND = "summary";
    expect(resolveWindowFromRuntime()).toBe("summary");
    delete (window as Window & { __SNAPPARSE_WINDOW_KIND?: string }).__SNAPPARSE_WINDOW_KIND;
  });
});
