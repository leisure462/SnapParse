# Auto-Detect Blank-Area Guard Design

## Scope

Fix the selection bar auto-detect false positives that still appear when the user drags across:

- desktop blank space
- webpage blank regions

while keeping intentional repeated text selections as available as practical.

## Chosen Approach

Tighten the Windows selection-surface heuristic in `src-tauri/src/lib.rs`.

- Replace the old boolean "looks like a text origin" check with a confidence model:
  - `Strong`: native edit-style text controls
  - `Weak`: generic web/render surfaces
  - `None`: non-text surfaces such as desktop blank space
- Require both drag start and drag end points to land on a non-`None` text surface.
- Keep the existing same-clipboard fallback only for deliberate drags that begin and end on `Strong` text surfaces.

## Why This Approach

- It directly targets the false positive path without changing the selection bar UI or Tauri window flow.
- It preserves the most reliable repeated-selection behavior for native editors while avoiding stale clipboard replays from blank areas.
- It is small enough for a release fix and easy to validate with the existing local checks.

## Validation Plan

- Run `npm run build`
- Run `cargo check --manifest-path src-tauri/Cargo.toml`
- Bump release metadata to `2.0.13`
- Push tag `v2.0.13` to trigger the GitHub release workflow
