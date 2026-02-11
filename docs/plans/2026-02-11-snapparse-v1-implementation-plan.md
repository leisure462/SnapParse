# SnapParse V1 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a Windows-first Tauri v2 text-selection assistant that shows an action bar after text selection and opens feature windows (translate, summarize, explain), with a sidebar settings page whose first tab is API configuration, Material Design 2 visual language, and a light/dark theme toggle in the toolbar.

**Architecture:** Use Tauri v2 multi-window architecture. Rust handles global mouse monitoring, selected-text capture, window lifecycle, OpenAI-compatible API calls, and settings persistence. React + TypeScript handles all window UIs, Material Design 2 tokens/components, theme switching (light/dark), and state synchronization via Tauri events/commands.

**Tech Stack:** Tauri v2, Rust, React, TypeScript, Vite, Vitest, GitHub Actions.

---

### Task 1: Bootstrap Empty Repository

**Files:**
- Create: `package.json`
- Create: `pnpm-lock.yaml` (generated in CI)
- Create: `tsconfig.json`
- Create: `vite.config.ts`
- Create: `index.html`
- Create: `src/main.tsx`
- Create: `src/App.tsx`
- Create: `src/styles/global.css`
- Create: `src-tauri/Cargo.toml`
- Create: `src-tauri/build.rs`
- Create: `src-tauri/tauri.conf.json`
- Create: `src-tauri/src/main.rs`

**Step 1: Write failing smoke tests for frontend and backend entrypoints**

```ts
// src/App.test.tsx
import { describe, it, expect } from 'vitest'
import { render, screen } from '@testing-library/react'
import App from './App'

describe('App', () => {
  it('renders SnapParse shell', () => {
    render(<App />)
    expect(screen.getByText('SnapParse')).toBeInTheDocument()
  })
})
```

```rust
// src-tauri/src/main.rs (test module)
#[cfg(test)]
mod tests {
    #[test]
    fn app_name_constant_exists() {
        assert_eq!("SnapParse", crate::APP_NAME);
    }
}
```

**Step 2: Run tests to verify they fail**

Run: `pnpm test -- App.test.tsx` and `cargo test -p snapparse-tauri`  
Expected: FAIL because project scaffold is not implemented yet.

**Step 3: Implement minimal project scaffold**

- Add Vite React app shell.
- Add Tauri v2 config and basic Rust startup.
- Keep all commands no-op initially.

**Step 4: Run tests to verify pass in CI context**

Run: `pnpm test -- App.test.tsx` and `cargo test -p snapparse-tauri`  
Expected: PASS.

**Step 5: Commit**

```bash
git add package.json tsconfig.json vite.config.ts index.html src src-tauri
git commit -m "chore: scaffold tauri v2 snapparse workspace"
```

---

### Task 2: Define Shared Settings Schema (Plaintext Storage)

**Files:**
- Create: `src/shared/settings.ts`
- Create: `src/shared/settings.test.ts`
- Create: `src-tauri/src/settings/mod.rs`
- Create: `src-tauri/src/settings/model.rs`
- Create: `src-tauri/src/settings/store.rs`

**Step 1: Write failing tests for default settings and validation**

```ts
import { describe, it, expect } from 'vitest'
import { defaultSettings, validateSettings } from './settings'

describe('settings schema', () => {
  it('has api section first and enabled action defaults', () => {
    const s = defaultSettings()
    expect(s.api.model).not.toBe('')
    expect(s.toolbar.actions[0].id).toBe('translate')
  })

  it('rejects invalid baseUrl', () => {
    expect(() => validateSettings({ api: { baseUrl: 'abc' } as any })).toThrow()
  })
})
```

**Step 2: Run tests to verify fail**

Run: `pnpm test -- settings.test.ts`  
Expected: FAIL because schema code does not exist.

**Step 3: Implement schema on frontend and mirrored Rust structs**

- Define sections in required order: `api`, `toolbar`, `window`, `features`, `advanced`.
- Include plaintext `apiKey` string field (per user decision).
- Include toolbar theme fields: `themeMode` (`light|dark|system`) and `showThemeToggleInToolbar`.
- Add `serde` structs matching frontend schema.

**Step 4: Run tests**

Run: `pnpm test -- settings.test.ts` and `cargo test -p snapparse-tauri settings`  
Expected: PASS.

**Step 5: Commit**

```bash
git add src/shared/settings.ts src/shared/settings.test.ts src-tauri/src/settings
git commit -m "feat: add typed settings schema with plaintext api config"
```

---

### Task 3: Implement Settings Persistence and Commands

**Files:**
- Modify: `src-tauri/src/main.rs`
- Create: `src-tauri/src/commands/settings.rs`
- Create: `src-tauri/src/commands/mod.rs`
- Create: `src-tauri/src/tests/settings_store_tests.rs`

**Step 1: Write failing Rust tests for read/write roundtrip**

```rust
#[test]
fn settings_roundtrip_persists_plaintext_api_key() {
    let mut s = AppSettings::default();
    s.api.api_key = "sk-test-123".into();
    save_settings_for_test(&s).unwrap();
    let loaded = load_settings_for_test().unwrap();
    assert_eq!(loaded.api.api_key, "sk-test-123");
}
```

**Step 2: Run tests to verify fail**

Run: `cargo test -p snapparse-tauri settings_roundtrip_persists_plaintext_api_key`  
Expected: FAIL before store implementation.

**Step 3: Implement persistence and Tauri commands**

- Store file at app config dir: `SnapParse/settings.json`.
- Commands: `get_settings`, `save_settings`, `reset_settings`.
- Add basic file-lock or mutex guarding concurrent writes.

**Step 4: Run tests**

Run: `cargo test -p snapparse-tauri settings_store`  
Expected: PASS.

**Step 5: Commit**

```bash
git add src-tauri/src/main.rs src-tauri/src/commands src-tauri/src/tests/settings_store_tests.rs
git commit -m "feat: persist settings and expose tauri settings commands"
```

---

### Task 4: Window Manager and Multi-Window Registration

**Files:**
- Create: `src-tauri/src/windows/mod.rs`
- Create: `src-tauri/src/windows/ids.rs`
- Create: `src-tauri/src/windows/manager.rs`
- Modify: `src-tauri/src/main.rs`
- Create: `src/windows/router.ts`

**Step 1: Write failing tests for window id mapping and route dispatch**

```ts
import { describe, it, expect } from 'vitest'
import { resolveWindowRoute } from './router'

describe('window router', () => {
  it('resolves summary window route', () => {
    expect(resolveWindowRoute('summary')).toBe('/windows/summary')
  })
})
```

**Step 2: Run tests to verify fail**

Run: `pnpm test -- router.test.ts`  
Expected: FAIL.

**Step 3: Implement window manager**

- Register windows: action bar, translate, summary, explain, settings.
- Add helper to show/focus/position windows.
- Keep all windows frameless style-ready; settings can be resizable.

**Step 4: Run tests**

Run: `pnpm test -- router.test.ts` and `cargo test -p snapparse-tauri window`  
Expected: PASS.

**Step 5: Commit**

```bash
git add src-tauri/src/windows src/windows/router.ts
git commit -m "feat: add multi-window manager and route mapping"
```

---

### Task 5: Selection Monitoring Pipeline (Windows)

**Files:**
- Create: `src-tauri/src/selection/mod.rs`
- Create: `src-tauri/src/selection/monitor.rs`
- Create: `src-tauri/src/selection/state.rs`
- Modify: `src-tauri/src/main.rs`
- Create: `src-tauri/src/tests/selection_judge_tests.rs`

**Step 1: Write failing tests for selection event judgment logic**

```rust
#[test]
fn drag_release_above_threshold_is_selection_candidate() {
    let r = judge_release(0, 0, 28, 0, 420, 900, 1700);
    assert!(r.is_selection_candidate);
}

#[test]
fn tiny_move_with_short_interval_is_double_click_selection() {
    let r = judge_release(100, 100, 104, 102, 120, 1000, 1500);
    assert!(r.is_selection_candidate);
}
```

**Step 2: Run tests to verify fail**

Run: `cargo test -p snapparse-tauri selection_judge_tests`  
Expected: FAIL.

**Step 3: Implement monitoring using documented flow**

- Hook mouse press/release.
- Track global state: previous press/release time, previous release position, selected text cache.
- Selection candidate -> capture text -> show action bar near cursor.
- Non-selection and non-actionbar-click -> hide action bar.

**Step 4: Run tests**

Run: `cargo test -p snapparse-tauri selection_judge_tests`  
Expected: PASS.

**Step 5: Commit**

```bash
git add src-tauri/src/selection src-tauri/src/tests/selection_judge_tests.rs src-tauri/src/main.rs
git commit -m "feat: implement windows text selection monitoring pipeline"
```

---

### Task 6: Action Bar Window UI and Actions

**Files:**
- Create: `src/windows/action-bar/ActionBarWindow.tsx`
- Create: `src/windows/action-bar/actionBar.css`
- Create: `src/windows/action-bar/actions.ts`
- Create: `src/windows/theme/themeStore.ts`
- Create: `src/windows/theme/themeTokens.css`
- Modify: `src/main.tsx`
- Create: `src/windows/action-bar/ActionBarWindow.test.tsx`

**Step 1: Write failing UI tests for action visibility and click events**

```ts
it('renders default five actions', () => {
  render(<ActionBarWindow />)
  expect(screen.getByText('翻译')).toBeInTheDocument()
  expect(screen.getByText('总结')).toBeInTheDocument()
})

it('renders toolbar theme toggle and flips mode', async () => {
  render(<ActionBarWindow />)
  const toggle = screen.getByRole('switch', { name: '明暗切换' })
  expect(toggle).toBeInTheDocument()
})
```

**Step 2: Run tests to verify fail**

Run: `pnpm test -- ActionBarWindow.test.tsx`  
Expected: FAIL.

**Step 3: Implement action bar UI**

- Horizontal floating bar with icon + label in Material Design 2 style (rounded surface, elevation, ripple/ink feedback).
- Actions: 翻译 / 解释 / 总结 / 搜索 / 复制.
- Add toolbar theme toggle switch (明/暗) and persist mode to settings.
- Click emits command/event to open the corresponding window.

**Step 4: Run tests**

Run: `pnpm test -- ActionBarWindow.test.tsx`  
Expected: PASS.

**Step 5: Commit**

```bash
git add src/windows/action-bar src/main.tsx
git commit -m "feat: add floating action bar window for selected text"
```

---

### Task 7: Translate and Summary Windows

**Files:**
- Create: `src/windows/translate/TranslateWindow.tsx`
- Create: `src/windows/translate/translate.css`
- Create: `src/windows/summary/SummaryWindow.tsx`
- Create: `src/windows/summary/summary.css`
- Create: `src/windows/common/WindowHeader.tsx`
- Create: `src/windows/common/ResultPanel.tsx`
- Create: `src/windows/translate/TranslateWindow.test.tsx`
- Create: `src/windows/summary/SummaryWindow.test.tsx`

**Step 1: Write failing tests for text injection and loading states**

```ts
it('shows original text after change-text event', async () => {
  // simulate injected selected text event
})

it('shows loading then result for summarize request', async () => {
  // simulate invoke lifecycle
})
```

**Step 2: Run tests to verify fail**

Run: `pnpm test -- TranslateWindow.test.tsx SummaryWindow.test.tsx`  
Expected: FAIL.

**Step 3: Implement windows and shared components**

- Follow Material Design 2 window style in both light and dark themes.
- Use MD2 layout/elevation/motion tokens with Chinese-first typography (`Noto Sans SC` fallback stack), avoiding generic default font stacks.
- Header with pin/opacity/minimize/close interactions.
- Result area supports "显示原文" toggle.

**Step 4: Run tests**

Run: `pnpm test -- TranslateWindow.test.tsx SummaryWindow.test.tsx`  
Expected: PASS.

**Step 5: Commit**

```bash
git add src/windows/translate src/windows/summary src/windows/common
git commit -m "feat: implement translate and summary windows"
```

---

### Task 8: OpenAI-Compatible AI Engine

**Files:**
- Create: `src-tauri/src/ai/mod.rs`
- Create: `src-tauri/src/ai/client.rs`
- Create: `src-tauri/src/ai/prompts.rs`
- Create: `src-tauri/src/commands/ai.rs`
- Modify: `src-tauri/src/commands/mod.rs`
- Create: `src-tauri/src/tests/ai_payload_tests.rs`

**Step 1: Write failing Rust tests for request payload and task prompt mapping**

```rust
#[test]
fn summarize_task_uses_summary_prompt_template() {
    let p = build_prompt(TaskKind::Summarize, "hello");
    assert!(p.system.contains("summary"));
}
```

**Step 2: Run tests to verify fail**

Run: `cargo test -p snapparse-tauri ai_payload_tests`  
Expected: FAIL.

**Step 3: Implement AI command layer**

- Command: `process_selected_text(task_kind, text, options)`.
- Use settings `base_url`, `api_key`, `model`, timeout.
- Return structured success/error payload for frontend rendering.

**Step 4: Run tests**

Run: `cargo test -p snapparse-tauri ai_payload_tests`  
Expected: PASS.

**Step 5: Commit**

```bash
git add src-tauri/src/ai src-tauri/src/commands/ai.rs src-tauri/src/commands/mod.rs src-tauri/src/tests/ai_payload_tests.rs
git commit -m "feat: add openai-compatible ai processing engine"
```

---

### Task 9: Sidebar Settings UI (API First)

**Files:**
- Create: `src/windows/settings/SettingsWindow.tsx`
- Create: `src/windows/settings/settings.css`
- Create: `src/windows/settings/sections/ApiSettings.tsx`
- Create: `src/windows/settings/sections/ToolbarSettings.tsx`
- Create: `src/windows/settings/sections/WindowSettings.tsx`
- Create: `src/windows/settings/sections/FeatureSettings.tsx`
- Create: `src/windows/settings/sections/AdvancedSettings.tsx`
- Create: `src/windows/settings/SettingsWindow.test.tsx`

**Step 1: Write failing tests for section order and default tab**

```ts
it('opens API settings as default first section', () => {
  render(<SettingsWindow />)
  expect(screen.getByRole('tab', { name: 'API配置' })).toHaveAttribute('aria-selected', 'true')
})

it('shows theme controls inside 工具栏 section', () => {
  render(<SettingsWindow />)
  expect(screen.getByLabelText('工具栏明暗切换')).toBeInTheDocument()
})
```

**Step 2: Run tests to verify fail**

Run: `pnpm test -- SettingsWindow.test.tsx`  
Expected: FAIL.

**Step 3: Implement sidebar settings page**

- Left sidebar navigation (not long vertical form page).
- Section order fixed: API配置 -> 工具栏 -> 功能窗口 -> 功能 -> 高级设置.
- Add theme controls in 工具栏: default mode (`light|dark|system`) and show/hide toggle on action bar.
- API key input supports masked display toggle.

**Step 4: Run tests**

Run: `pnpm test -- SettingsWindow.test.tsx`  
Expected: PASS.

**Step 5: Commit**

```bash
git add src/windows/settings
git commit -m "feat: add sidebar settings with api config as first section"
```

---

### Task 10: App Icon, Packaging Metadata, and Workflow

**Files:**
- Modify: `src-tauri/tauri.conf.json`
- Create: `src-tauri/icons/icon.ico` (copy from `/_ Copy 3.ico`)
- Create: `.github/workflows/ci.yml`
- Create: `.github/workflows/release.yml`
- Create: `README.md`

**Step 1: Write failing workflow lint checks**

- Add a minimal workflow validation step to ensure required files exist.

**Step 2: Run check in workflow context**

Run: `act -n` (optional locally) or rely on GitHub Actions push check.  
Expected: Initially FAIL until workflow paths are correct.

**Step 3: Implement workflows**

- `ci.yml`: frontend tests + rust tests + format/lint checks.
- `release.yml`: Windows build and artifact upload.
- Include environment variable usage for runtime API config (no hardcoded secrets).

**Step 4: Verify workflow definitions**

Run: GitHub Actions on PR.  
Expected: PASS on `windows-latest`.

**Step 5: Commit**

```bash
git add src-tauri/tauri.conf.json src-tauri/icons .github/workflows README.md
git commit -m "chore: add app icon and github actions build pipelines"
```

---

### Task 11: Final Integration and Manual QA Script

**Files:**
- Create: `docs/manual-qa.md`
- Modify: `README.md`

**Step 1: Write failing checklist by defining acceptance criteria before verification**

- Selection action bar appears for drag selection and double-click selection.
- Clicking each action performs expected behavior.
- Settings sidebar opens with API tab selected by default.
- Toolbar light/dark toggle updates all open windows immediately and persists after restart.

**Step 2: Execute CI verification**

Run: complete pipeline from PR.  
Expected: All checks green.

**Step 3: Document manual QA steps**

- Include test text scenarios in browser, document editor, and code editor.
- Include negative cases (empty selection, unsupported apps).

**Step 4: Final verification**

Run: one full workflow from clean branch and confirm artifact generation.

**Step 5: Commit**

```bash
git add docs/manual-qa.md README.md
git commit -m "docs: add integration verification and manual qa checklist"
```

---

## Deployment and Push Strategy

1. Initialize git in local workspace if needed.
2. Add remote: `https://github.com/leisure462/SnapParse.git`.
3. Create feature branch: `feat/snapparse-v1`.
4. Execute tasks sequentially with small commits.
5. Push branch and open PR with screenshots and workflow links.

## Known Trade-Offs (Accepted for V1)

- API key is stored in plaintext `settings.json` (user-approved), so include clear warning in UI.
- Windows-first behavior takes priority; macOS/Linux support can be added in V2.
- Selection text extraction reliability varies by app; include fallback and clear error messages.
- Material Design 2 is used for visual system; component set is custom-built (not importing full MUI runtime) to keep bundle lean.
