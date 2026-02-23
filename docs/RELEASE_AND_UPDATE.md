# Release and Update Guide

This document explains:

1. How to publish SnapParse releases
2. How to deliver future updates to users
3. How to enable in-app hot update (auto updater)

---

## 1. Release Packaging

Build installers locally:

```bash
npm run tauri build
```

Expected outputs:

- MSI: `src-tauri/target/release/bundle/msi/*.msi`
- NSIS: `src-tauri/target/release/bundle/nsis/*-setup.exe`

Recommended release checklist:

- Bump version in:
  - `package.json`
  - `src-tauri/tauri.conf.json`
  - `src-tauri/Cargo.toml`
- Build and smoke test
- Upload installers to GitHub Releases
- Update changelog/release notes

---

## 2. How to Push New Changes to Users (Without In-App Updater)

If updater is not enabled yet:

- Publish a new GitHub Release version
- Upload new installer files
- Users download and install the new version manually

This is the simplest and most stable approach for early-stage releases.

---

## 3. Can SnapParse Support Hot Update?

Yes. Tauri v2 supports updater-based in-app update flow.

Typical flow:

1. User app checks update metadata endpoint
2. App finds a newer version
3. App downloads signed update package
4. App installs and relaunches

---

## 4. Tauri v2 Updater Implementation Outline

### Step A: Enable updater plugin

Add updater plugin in Rust setup and configure permissions/capabilities as needed.

### Step B: Configure updater endpoint

In `tauri.conf.json`, define updater configuration and endpoint URL.

### Step C: Prepare signing key

Generate and securely store updater signing private key.

- Private key: used only in CI/release signing
- Public key: embedded in app for verification

### Step D: Release pipeline

In CI/CD:

- Build release bundle
- Sign update artifacts
- Publish metadata manifest + files

### Step E: Frontend update check

Expose a UI action (for example in Settings > About):

- Check for updates
- Show update details
- Download + install

---

## 5. Suggested Delivery Strategy

### Stage 1 (current)
- Manual installer updates via GitHub Releases

### Stage 2
- Add "Check for updates" button (manual trigger)

### Stage 3
- Add silent background periodic checks with user consent

---

## 6. Security Notes

- Never ship updater private key in repository
- Always verify signatures before install
- Use HTTPS for update metadata and files
- Keep rollback strategy for failed releases

---

## 7. Recommended GitHub Release Assets

- `SnapParse_<version>_x64_en-US.msi`
- `SnapParse_<version>_x64-setup.exe`
- `SHA256SUMS.txt`
- Release notes (fixes/features/known issues)
