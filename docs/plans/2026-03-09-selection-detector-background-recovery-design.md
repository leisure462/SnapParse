# Selection Detector Background Recovery Design

## Scope

Fix the selection assistant so it can still trigger reliably after the app has been running in the
background for a long time.

- Cover both `auto-detect` and `copy-trigger`
- Focus on the shared Rust-side detection lifecycle
- Ship the fix as a new release

## Chosen Approach

Use a small self-healing update in the Tauri backend.

- Treat SnapParse windows as blocking detection only when they are both visible and focused
- Add a detector-thread drop guard so abnormal worker exit clears `detector_running` and the
  heartbeat immediately
- Wrap the detector worker loop in `catch_unwind` so panics are logged and the guard can hand
  control back to the existing watchdog restart path

## Why This Approach

- Both trigger modes share the same background detector thread, so a lifecycle fix addresses both
  symptoms at once
- Hidden window focus state is a plausible long-idle false positive that can stop all detection
- Reusing the existing heartbeat watchdog keeps the patch small and low risk instead of introducing
  a larger supervisor refactor

## Validation Plan

- Run `cargo fmt --manifest-path src-tauri/Cargo.toml`
- Run `npm run build`
- Run `cargo check --manifest-path src-tauri/Cargo.toml`
- Bump release metadata to `2.0.18`
- Commit, tag `v2.0.18`, and push to trigger the GitHub release workflow
