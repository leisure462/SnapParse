import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";
import App from "./App";

describe("App", () => {
  it("renders SnapParse shell", () => {
    render(<App />);
    expect(screen.getByText("SnapParse")).toBeInTheDocument();
  });
});
