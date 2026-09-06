# PowerShell build script for SnapParse Qt
$ErrorActionPreference = "Stop"

# Terminate any running instance of SnapParse or EcoPaste before building
Get-Process | Where-Object { $_.Name -like "*SnapParse*" -or $_.Name -like "*EcoPaste*" } -ErrorAction SilentlyContinue | ForEach-Object {
    try { $_.Kill(); $_.WaitForExit(1000) } catch {}
}

$qtRoot = "D:\dev\Qt\6.8.2\msvc2022_64"
$msvcVars = "D:\dev\C++\VC\Auxiliary\Build\vcvars64.bat"
$cmakeBin = "D:\software\install\cmake\bin"
$ninjaBin = "C:\msys64\ucrt64\bin"

Write-Host "[SnapParse Qt] Initializing MSVC 2022 & Qt 6.8.2 Build Environment..." -ForegroundColor Cyan

# 1. MSVC environment
if (Test-Path $msvcVars) {
    cmd.exe /c "call `"$msvcVars`" && set" | ForEach-Object {
        if ($_ -match '^(.*?)=(.*)$') {
            [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
}

# 2. Path variables
if ($env:Path -notlike "*$cmakeBin*") { $env:Path = "$cmakeBin;" + $env:Path }
if ($env:Path -notlike "*$ninjaBin*") { $env:Path = "$ninjaBin;" + $env:Path }
if ($env:Path -notlike "*$qtRoot\bin*") { $env:Path = "$qtRoot\bin;" + $env:Path }

$env:Qt6_DIR = "$qtRoot\lib\cmake\Qt6"
$env:QT_DIR = $qtRoot
$env:CMAKE_PREFIX_PATH = "$qtRoot;" + $env:CMAKE_PREFIX_PATH

$buildDir = "d:\play\SnapParse\src-qt\build"
if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

Set-Location $buildDir

Write-Host "[CMake] Configuring project..." -ForegroundColor Cyan
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..

Write-Host "[Ninja] Building SnapParse executable..." -ForegroundColor Cyan
ninja

Write-Host "[windeployqt] Deploying runtime dependencies..." -ForegroundColor Cyan
& "$qtRoot\bin\windeployqt.exe" --release --no-translations "$buildDir\SnapParse.exe"

# Prune unneeded plugins (ODBC, PostgreSQL, Mimer) to save space
$unneededDlls = @("sqldrivers\qsqlmimer.dll", "sqldrivers\qsqlodbc.dll", "sqldrivers\qsqlpsql.dll", "opengl32sw.dll", "d3dcompiler_47.dll")
foreach ($dll in $unneededDlls) {
    $p = Join-Path $buildDir $dll
    if (Test-Path $p) { Remove-Item -Force $p -ErrorAction SilentlyContinue }
}

Write-Host "[Success] Build and deployment completed successfully!" -ForegroundColor Green
