import { readFileSync, writeFileSync, existsSync, mkdirSync } from "node:fs";
import { basename, dirname, join } from "node:path";

const workspaceRoot = process.cwd();
const packageJsonPath = join(workspaceRoot, "package.json");
const packageJson = JSON.parse(readFileSync(packageJsonPath, "utf8"));

const version = String(packageJson.version || "").trim();
if (!version) {
  throw new Error("Cannot read version from package.json");
}

const repo = process.env.UPDATER_REPOSITORY || "leisure462/SnapParse";
const tag = process.env.UPDATER_TAG || `v${version}`;
const releaseBaseUrl =
  process.env.UPDATER_RELEASE_BASE_URL ||
  `https://github.com/${repo}/releases/download/${tag}`;
const notes = (process.env.UPDATER_NOTES || "").trim();
const pubDate = process.env.UPDATER_PUB_DATE || new Date().toISOString();

const bundleRoot = join(workspaceRoot, "src-tauri", "target", "release", "bundle");
const manifestPath = join(bundleRoot, "latest.json");

const candidates = [
  {
    target: "windows-x86_64-msi",
    archivePath: join(bundleRoot, "msi", `SnapParse_${version}_x64_en-US.msi.zip`),
  },
  {
    target: "windows-x86_64-nsis",
    archivePath: join(bundleRoot, "nsis", `SnapParse_${version}_x64-setup.nsis.zip`),
  },
];

const platforms = {};
for (const item of candidates) {
  if (!existsSync(item.archivePath)) {
    continue;
  }
  const sigPath = `${item.archivePath}.sig`;
  if (!existsSync(sigPath)) {
    throw new Error(`Missing signature file: ${sigPath}`);
  }
  const fileName = basename(item.archivePath);
  const signature = readFileSync(sigPath, "utf8").trim();
  platforms[item.target] = {
    url: `${releaseBaseUrl}/${fileName}`,
    signature,
  };
}

if (!platforms["windows-x86_64"]) {
  if (platforms["windows-x86_64-nsis"]) {
    platforms["windows-x86_64"] = platforms["windows-x86_64-nsis"];
  } else if (platforms["windows-x86_64-msi"]) {
    platforms["windows-x86_64"] = platforms["windows-x86_64-msi"];
  }
}

if (Object.keys(platforms).length === 0) {
  throw new Error(
    "No updater artifacts found. Please run `npm run tauri build` first and ensure v1 compatible updater artifacts are generated."
  );
}

const manifest = {
  version,
  notes,
  pub_date: pubDate,
  platforms,
};

mkdirSync(dirname(manifestPath), { recursive: true });
writeFileSync(manifestPath, `${JSON.stringify(manifest, null, 2)}\n`, "utf8");

console.log(`Updater manifest generated: ${manifestPath}`);
console.log(
  `Release base URL: ${releaseBaseUrl}\nTargets: ${Object.keys(platforms).join(", ")}`
);
