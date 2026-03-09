# Selection Result Corner Artifact Design

## Scope

Fix the visible corner ghosting that appears only on the selection result window.

- Do not change OCR result window styling.
- Keep the existing rounded text output panel and current behavior intact.

## Chosen Approach

Apply a final CSS-only override that makes the selection result window root fully transparent and
lets `selection-result-shell` become the single visible rounded surface.

- Reapply one shared radius token to the selection-result root and shell.
- Remove the outer gradient/background layer from the selection-result root.
- Clip the selection-result root to the same rounded rectangle as the visible shell.

## Why This Approach

- The artifact is caused by layered transparent backgrounds with mismatched corner ownership.
- A targeted selection-result-only CSS fix is the lowest-risk way to remove the ghosting.
- OCR stays untouched because it does not currently show the defect.

## Validation Plan

- Run `npm run build`
- Run `cargo check --manifest-path src-tauri/Cargo.toml`
- Bump release metadata to `2.0.14`
- Push tag `v2.0.14` to trigger the GitHub release workflow
