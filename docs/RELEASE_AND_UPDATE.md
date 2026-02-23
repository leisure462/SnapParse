# Release and Auto-Update Guide

This project has enabled **Tauri v2 updater** end-to-end:

- Rust plugin: updater + process relaunch
- Frontend update flow: check -> download -> install -> relaunch
- Configured endpoint: GitHub Releases `latest.json`

## 1. One-Time Setup

### 1.1 Keep signing key safe

Updater signing key files are generated locally:

- Private key: `src-tauri/updater.key` (must be kept secret)
- Public key: `src-tauri/updater.key.pub` (already embedded into `tauri.conf.json`)

Do not commit private key to repository.

### 1.2 Environment variables for release signing

Before building release, set:

- `TAURI_SIGNING_PRIVATE_KEY` (private key content, not file path)
- `TAURI_SIGNING_PRIVATE_KEY_PASSWORD`

PowerShell example:

```powershell
$env:TAURI_SIGNING_PRIVATE_KEY = Get-Content -Raw "D:\play\SnapParse\src-tauri\updater.key"
$env:TAURI_SIGNING_PRIVATE_KEY_PASSWORD = "your-password"
```

## 2. Release Build

Ensure version is updated in:

- `package.json`
- `src-tauri/tauri.conf.json`
- `src-tauri/Cargo.toml`

Then build:

```bash
npm run tauri build
```

With `createUpdaterArtifacts: "v1Compatible"`, build output includes updater artifacts and `latest.json`.

Typical output folders:

- MSI: `src-tauri/target/release/bundle/msi/`
- NSIS: `src-tauri/target/release/bundle/nsis/`
- Updater artifacts: `.zip` + `.sig` files under `bundle/msi` and `bundle/nsis`

Generate `latest.json`:

```bash
npm run updater:manifest
```

Output:

- `src-tauri/target/release/bundle/latest.json`

## 3. GitHub Release Upload

Create a new release tag (for example `v1.0.1`) and upload:

- Installer (`.msi` / setup `.exe`)
- Updater metadata: `latest.json`
- Updater payload/signature:
  - `SnapParse_<version>_x64_en-US.msi.zip`
  - `SnapParse_<version>_x64_en-US.msi.zip.sig`
  - `SnapParse_<version>_x64-setup.nsis.zip`
  - `SnapParse_<version>_x64-setup.nsis.zip.sig`

Important:

- `latest.json` must be uploaded as a release asset
- `latest.json` URLs must point to the same release tag assets
- Default generator uses tag `v<version>` (for example `v1.0.1`)
- Current app endpoint:
  - `https://github.com/leisure462/SnapParse/releases/latest/download/latest.json`

## 4. Client Update Flow

In app (`关于与诊断`):

1. Click `检查更新`
2. If update exists, click `下载并安装`
3. Wait for progress
4. App auto relaunches after install

## 5. Notes for CI/CD

You can later move release build to GitHub Actions:

- Inject signing key/path/password via repository secrets
- Build on tag push
- Upload `latest.json` + signed artifacts automatically

### Included workflow in this repository

File:

- `.github/workflows/release.yml`

Trigger:

- Push tag `v*` (recommended for official releases)
- Manual run (`workflow_dispatch`) with `release_tag`

Required repository secrets:

- `TAURI_SIGNING_PRIVATE_KEY`
- `TAURI_SIGNING_PRIVATE_KEY_PASSWORD`

Release assets uploaded by workflow:

- MSI/NSIS installer
- updater zip packages + `.sig`
- `latest.json`

## 6. Security Checklist

- Never expose `updater.key`
- Rotate key only if strictly necessary (key rotation invalidates old signing trust chain)
- Use HTTPS endpoints only
- Keep release notes and rollback installers for emergency recovery
