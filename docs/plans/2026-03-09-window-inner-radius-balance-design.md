# Window Inner Radius Balance Design

## Scope

Fix the remaining corner layering artifacts caused by inner visible fill elements using radii that
are too close to, or larger than, their host window radii.

- Apply to the selection result window
- Apply to the clipboard window
- Do not change OCR result window styling

## Chosen Approach

Use final CSS-only overrides to establish a strict radius hierarchy:

- outer window radius
- inner shell radius
- inner content/card/control radius

Each inner visible fill layer is reduced by 1 to 2 pixels relative to its parent.

## Why This Approach

- The reported artifact matches radius ownership conflicts between transparent outer shells and
  visible inner fill layers.
- A radius hierarchy is the smallest safe change that addresses both windows without changing
  behavior or React structure.
- OCR stays untouched because it does not currently show the issue.

## Validation Plan

- Run `npm run build`
- Run `cargo check --manifest-path src-tauri/Cargo.toml`
- Bump release metadata to `2.0.15`
- Push tag `v2.0.15` to trigger the GitHub release workflow
