# PowerShell Script to Package SnapParse Single-File Portable Executable (.exe)
$ErrorActionPreference = "Stop"

$RootDir = $PSScriptRoot
$BuildDir = Join-Path $RootDir "build"
$DistDir = Join-Path $RootDir "dist"
$StagingDir = Join-Path $DistDir "staging"
$LauncherDir = Join-Path $RootDir "tools\launcher"
$ReleaseDir = Join-Path $RootDir "release"

Write-Host "=================================================" -ForegroundColor Cyan
Write-Host "  SnapParse Single-File Portable EXE Packager    " -ForegroundColor Cyan
Write-Host "=================================================" -ForegroundColor Cyan

# 1. Initialize MSVC Environment (if not already active)
Write-Host "[1/6] Initializing MSVC Compiler Environment..." -ForegroundColor Yellow
if (Get-Command "cl.exe" -ErrorAction SilentlyContinue) {
    Write-Host "  MSVC compiler already available in PATH." -ForegroundColor Green
} else {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found. Visual Studio must be installed."
    }

    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) {
        throw "Visual Studio C++ environment not found."
    }

    $vcvarsall = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $vcvarsall)) {
        throw "vcvarsall.bat not found at $vcvarsall"
    }

    # Import VC environment variables
    $tempBat = Join-Path $env:TEMP "import_vcvars_$PID.bat"
    "@echo off
call `"$vcvarsall`" x64 >nul
set
" | Set-Content -Path $tempBat

    $envLines = cmd /c $tempBat
    Remove-Item $tempBat -Force

    foreach ($line in $envLines) {
        if ($line -match "^([^=]+)=(.*)$") {
            [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
    Write-Host "  MSVC x64 Environment loaded successfully." -ForegroundColor Green
}

# 2. Stage distribution files
Write-Host "[2/6] Staging distribution files..." -ForegroundColor Yellow
if (Test-Path $StagingDir) { Remove-Item -Recurse -Force $StagingDir }
New-Item -ItemType Directory -Path $StagingDir -Force | Out-Null
if (-not (Test-Path $ReleaseDir)) { New-Item -ItemType Directory -Path $ReleaseDir -Force | Out-Null }

$requiredFiles = @(
    "SnapParse.exe",
    "Qt6Core.dll",
    "Qt6Gui.dll",
    "Qt6Network.dll",
    "Qt6Sql.dll",
    "Qt6Svg.dll",
    "Qt6Widgets.dll",
    "d3dcompiler_47.dll",
    "opengl32sw.dll"
)

foreach ($f in $requiredFiles) {
    $src = Join-Path $BuildDir $f
    if (Test-Path $src) {
        Copy-Item -Path $src -Destination $StagingDir
    }
}

$requiredDirs = @(
    "platforms",
    "sqldrivers",
    "imageformats",
    "iconengines",
    "styles",
    "tls",
    "networkinformation",
    "generic"
)

foreach ($d in $requiredDirs) {
    $src = Join-Path $BuildDir $d
    if (Test-Path $src) {
        Copy-Item -Path $src -Destination (Join-Path $StagingDir $d) -Recurse -Force
    }
}

# 3. Create high-compression payload.zip
Write-Host "[3/6] Compressing runtime payload into payload.zip..." -ForegroundColor Yellow
$PayloadZip = Join-Path $LauncherDir "payload.zip"
if (Test-Path $PayloadZip) { Remove-Item -Force $PayloadZip }

$sevenZip = "C:\Program Files\7-Zip\7z.exe"
if (Test-Path $sevenZip) {
    Push-Location $StagingDir
    & $sevenZip a -tzip -mx=9 $PayloadZip * | Out-Null
    Pop-Location
} else {
    Push-Location $StagingDir
    tar.exe -a -cf $PayloadZip *
    Pop-Location
}

$zipSize = (Get-Item $PayloadZip).Length / 1MB
Write-Host ("  Payload compressed: {0:N2} MB" -f $zipSize) -ForegroundColor Green

# 4. Copy Icon and Compile Windows Resources (.rc -> .res)
Write-Host "[4/6] Compiling application icon and metadata resources..." -ForegroundColor Yellow
$IconSrc = Join-Path $RootDir "resources\icons\app_icon.ico"
$IconDst = Join-Path $LauncherDir "app_icon.ico"
Copy-Item -Path $IconSrc -Destination $IconDst -Force

Push-Location $LauncherDir
& rc.exe /nologo /fo "launcher.res" "launcher.rc"
if ($LASTEXITCODE -ne 0) {
    Pop-Location
    throw "Resource compilation failed!"
}

# 5. Compile and Link Single-File Executable
Write-Host "[5/6] Building single executable with MSVC..." -ForegroundColor Yellow
$OutExe = Join-Path $ReleaseDir "SnapParse.exe"
if (Test-Path $OutExe) { Remove-Item -Force $OutExe }

& cl.exe /nologo /O2 /Oi /Ot /Gy /GF /MT /utf-8 /std:c++20 /DUNICODE /D_UNICODE `
    "main.cpp" "launcher.res" `
    /link /SUBSYSTEM:WINDOWS `
    /MANIFEST:EMBED /MANIFESTINPUT:"launcher.manifest" `
    /OPT:REF /OPT:ICF `
    Shell32.lib Ole32.lib OleAut32.lib User32.lib Advapi32.lib `
    /OUT:"$OutExe"

if ($LASTEXITCODE -ne 0) {
    Pop-Location
    throw "Linking Single-File Executable failed!"
}
Pop-Location

# 6. Clean up temporary staging and intermediate build artifacts
Write-Host "[6/6] Cleaning up intermediate build files and staging..." -ForegroundColor Yellow
if (Test-Path $StagingDir) { Remove-Item -Recurse -Force $StagingDir }
if (Test-Path $DistDir) { Remove-Item -Recurse -Force $DistDir }
$tempPayload = Join-Path $LauncherDir "payload.zip"
$tempRes = Join-Path $LauncherDir "launcher.res"
$tempIco = Join-Path $LauncherDir "app_icon.ico"
if (Test-Path $tempPayload) { Remove-Item -Force $tempPayload }
if (Test-Path $tempRes) { Remove-Item -Force $tempRes }
if (Test-Path $tempIco) { Remove-Item -Force $tempIco }

# 7. Final verification and report
if (Test-Path $OutExe) {
    # Also sync to workspace root for convenience
    $RootExe = Join-Path $RootDir "..\SnapParse.exe"
    Copy-Item $OutExe $RootExe -Force

    $finalSize = (Get-Item $OutExe).Length / 1MB
    Write-Host "=================================================" -ForegroundColor Green
    Write-Host "  SnapParse Single-File Portable EXE Built!      " -ForegroundColor Green
    Write-Host ("  Release File: {0}" -f $OutExe) -ForegroundColor Cyan
    Write-Host ("  Root File   : {0}" -f (Resolve-Path $RootExe)) -ForegroundColor Cyan
    Write-Host ("  File Size   : {0:N2} MB" -f $finalSize) -ForegroundColor Green
    Write-Host "=================================================" -ForegroundColor Green
} else {
    throw "Output executable not found!"
}
