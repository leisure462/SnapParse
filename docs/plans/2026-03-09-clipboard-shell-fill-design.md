# Clipboard Shell Fill Design

## Scope

Fix the remaining clipboard window corner artifacts that still appear on the top-left, bottom-left,
and bottom-right corners.

- Apply only to the clipboard window
- Keep the existing selection result and OCR window styling unchanged

## Chosen Approach

Move edge spacing from child margins and list padding into the clipboard shell itself.

- Let `.clipboard-window` become the single inner fill shell
- Give the shell its own background and padding
- Remove child edge margins that were exposing the outer transparent layer near the corners

## Why This Approach

- The remaining artifact is caused by layout spacing pulling the visible content away from the
  window corners.
- A shell-owned padding model keeps the corners consistently filled without restructuring React.
- It is safer than adding more masking layers or touching unrelated windows.

## Validation Plan

- Run `npm run build`
- Run `cargo check --manifest-path src-tauri/Cargo.toml`
- Bump release metadata to `2.0.16`
- Push tag `v2.0.16` to trigger the GitHub release workflow
