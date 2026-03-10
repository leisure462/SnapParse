# OCR Capture Z-Order Reinforcement Design

## Scope

Fix the OCR capture overlay so it stays above other desktop apps when opened by the OCR shortcut.

- Keep the OCR capture mask non-activating
- Strengthen the overlay z-order when shown
- Add a small runtime-state self-heal for stale OCR capture sessions
- Keep the work local only until the user validates it

## Chosen Approach

Use a small Rust-side window-management patch in `src-tauri/src/lib.rs`.

- Add a helper that reasserts OCR capture window `show + focusable(false) + always_on_top`
  together with its target monitor position and size
- Call that helper immediately after the capture window is shown, then repeat it twice with short
  delays to outlast apps that briefly reclaim topmost order
- Add shared OCR capture runtime reset helpers so stale `capture_active` state and stale hidden
  overlays can be cleaned up before reopening

## Why This Approach

- The current flow sets `always_on_top` only once before `show()`, which is fragile against apps
  that promote themselves again right after the shortcut fires
- Reasserting the capture overlay without activating it preserves the current UX while making the
  mask much harder to bury behind special windows
- Unifying the OCR capture reset path reduces the chance of long-running background sessions leaving
  the app in an inconsistent capture state

## Validation Plan

- Run `cargo fmt --manifest-path src-tauri/Cargo.toml`
- Run `npm run build`
- Run `cargo check --manifest-path src-tauri/Cargo.toml`
- Do not commit, tag, or push until the user finishes local verification
