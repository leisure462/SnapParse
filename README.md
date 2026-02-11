# SnapParse

SnapParse is a Windows-first text selection assistant built with Tauri v2.

When you select text on screen, SnapParse shows a floating action bar. You can click actions such as translate, summarize, explain, search, or copy, then open the corresponding feature window to process the selected text.

## Current V1 Scope

- Tauri v2 multi-window architecture (Rust backend + React frontend)
- Windows selection monitoring pipeline
- Floating action bar with Material Design 2 style
- Toolbar light/dark toggle
- Translate, summary, and explain windows
- Sidebar settings page with API-first information architecture
- OpenAI-compatible API integration (`base_url + api_key + model`)
- Settings persistence (`settings.json` in app config directory)

## Settings Information Architecture

The settings sidebar uses this fixed order:

1. API配置
2. 工具栏
3. 功能窗口
4. 功能
5. 高级设置

## Security Note

Per current product decision, API key is stored in plaintext in local settings file. Keep your machine secure.

## GitHub Actions Build

This repository is configured for cloud build workflows:

- CI: `.github/workflows/ci.yml`
- Release: `.github/workflows/release.yml`

To create a release build:

1. Push a tag like `v0.1.0`
2. GitHub Actions runs the `release` workflow
3. Draft release assets are uploaded automatically

## Local Development (Optional)

If you want to run locally on a prepared machine:

```bash
npm install
npm run dev
```

Rust tests:

```bash
cargo test --manifest-path src-tauri/Cargo.toml
```

## Design Direction

- Visual language: Material Design 2
- Typography: Chinese-first sans stack with `Noto Sans SC` fallback
- Supports both light and dark themes

## Documentation

- Plan: `docs/plans/2026-02-11-snapparse-v1-implementation-plan.md`
- Manual QA: `docs/manual-qa.md`
