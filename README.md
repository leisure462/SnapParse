# SnapParse

SnapParse is a Windows desktop productivity tool built with **Tauri v2 + React + TypeScript**.

It combines clipboard management, text selection assistant, OCR workflow, and TTS playback in one app.

## Core Features

### Clipboard Panel
- Vertical history list for text, links, images, and favorites
- Click-to-copy workflow with optional auto-close
- Search, category filters, and compact header behavior
- Pin window on top, favorite items, and lightweight scrolling UI

### Selection Assistant
- Detects selected text and shows an action bar near the selection
- Built-in actions: `Copy`, `Summary`, `Polish`, `Translate`, `Explain`, `Search`
- Supports custom agents (name, icon, prompt)
- Supports action order, visibility control, and compact mode

### Smart OCR
- Trigger OCR region capture via global hotkey
- Crosshair-based region selection
- OCR text extraction via configurable vision model API
- Post-processing through assistant actions (translate/summary/polish/explain/custom)

### Processing Windows
- Dedicated result windows for Selection and OCR
- Streaming output support
- Copy, favorite, and TTS playback controls
- Auto-hide on blur and always-on-top behavior controls

### System Integration
- Tray icon and localized tray menu
- Global shortcuts
- Launch on startup and silent startup
- Persistent settings, import/export, and data storage controls
- Built-in updater (check/download/install/relaunch)

## Tech Stack

- Frontend: React 18, TypeScript, Vite, lucide-react
- Backend: Rust, Tauri v2
- Plugins: `tauri-plugin-global-shortcut`, `tauri-plugin-single-instance`, `tauri-plugin-autostart`

## Project Structure

```text
src/
  App.tsx
  styles/app.css
  types.ts

src-tauri/
  src/lib.rs
  tauri.conf.json
  capabilities/default.json

docs/
  RELEASE_AND_UPDATE.md
```

## Quick Start

### Prerequisites
- Node.js 18+
- Rust stable toolchain
- Windows build tools required by Tauri

### Install dependencies

```bash
npm install
```

### Run in development mode

```bash
npm run tauri dev
```

### Build installers

```bash
npm run tauri build
```

Build artifacts:
- `src-tauri/target/release/bundle/msi/`
- `src-tauri/target/release/bundle/nsis/`

## Settings Overview

Main setting groups:
- General
- Clipboard
- Selection Assistant
- Smart OCR
- TTS
- Shortcuts
- Data Backup
- About & Diagnostics

Common settings include:
- Launch on startup / silent startup
- Auto-close on blur (global behavior)
- Remember window size and position
- Action bar behavior and compact mode
- Model API configuration (LLM + OCR)
- Ignored apps list for selection assistant

## API Configuration

SnapParse supports:
- OpenAI-compatible chat/completion APIs for LLM processing
- Provider-specific OCR endpoints (including GLM OCR style responses)

Recommended minimum config:
- Base URL
- API key
- Model name
- Timeout / token limits

## TTS Runtime

TTS is integrated with Edge TTS runtime:
- Runtime initialization happens automatically
- Playback buttons are provided in Selection and OCR result windows
- Playback stops when result window is closed/hidden

## Open Source Contribution

- Use **Issues** for bug reports and feature discussions
- Use **Pull Requests** for code contributions
- Keep PRs focused and include test/repro notes

## Release & Update

See `docs/RELEASE_AND_UPDATE.md` for:
- Packaging and release checklist
- Versioning recommendations
- How to deliver future updates
- Tauri updater (hot update) implementation path
- Signing key and `latest.json` release workflow

Helper command:

```bash
npm run updater:manifest
```

GitHub Actions release workflow:
- `.github/workflows/release.yml`

User guide (Chinese):
- `docs/USER_GUIDE.zh-CN.md`

## License

This project is licensed under the MIT License.
See `LICENSE` for details.
