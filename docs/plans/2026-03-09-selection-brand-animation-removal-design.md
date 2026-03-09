# Selection Brand Animation Removal Design

## Scope

Remove the selection bar brand icon animation from the shipped product and release the change as a
new version.

- Remove the animation behavior from the selection bar icon
- Remove the corresponding settings toggle and type surface
- Keep unrelated clipboard corner experiments out of the release

## Chosen Approach

Use a small frontend-only cleanup.

- Remove `showIconAnimation` from the selection assistant settings model and normalization path
- Render the selection bar brand icon with a static class only
- Delete the legacy and hotfix CSS animation rules and keyframes tied to the brand icon

## Why This Approach

- The user no longer wants this animation behavior in the product
- Removing the setting and styling together avoids dead UI, dead state, and confusing persistence
- The change is isolated to the React and CSS layer, so it is low risk and easy to validate

## Validation Plan

- Run `npm run build`
- Run `cargo check --manifest-path src-tauri/Cargo.toml`
- Bump release metadata to `2.0.17`
- Commit, tag `v2.0.17`, and push to trigger the GitHub release workflow
