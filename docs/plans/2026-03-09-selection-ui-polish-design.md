# Selection UI Polish Design

## Scope

Apply the approved minimal-scope fix for two regressions in the selection assistant UI:

- The large text display area in the selection result window should use a rounded rectangle instead of a hard rectangular blur/shadow surface.
- The leading app icon in the selection bar should visibly pulse again.

## Chosen Approach

Use CSS-only targeted overrides at the end of `src/styles/app.css`.

- Keep the result window structure unchanged.
- Reapply a shared radius token to the result shell and the inner result display panel.
- Move the visible blur/surface treatment onto the result display container so the rounded clipping is stable.
- Restore the selection bar icon motion by making the brand container overflow visible and using a stronger but still subtle bob/breathe animation.

## Why This Approach

- Lowest regression risk for a release-bound fix.
- Avoids touching Tauri window logic and React rendering.
- Works with the existing late-file hotfix cascade by placing the final overrides at the end.

## Validation Plan

- Run `npm run build`
- Run `cargo check --manifest-path src-tauri/Cargo.toml`
- After validation, bump version metadata and push tag `v2.0.12` to trigger GitHub release build
