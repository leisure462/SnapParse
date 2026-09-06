# Cleanup script for temporary build files and caches
$ErrorActionPreference = "SilentlyContinue"

Write-Host "Cleaning temporary artifacts across C: and D: drives..." -ForegroundColor Yellow

# 1. Clean C: Drive temporary files
$cItems = @(
    "C:\Users\谢玉林\AppData\Local\SnapParseHub\SnapParse\runtime-v3.0.0",
    "C:\Users\谢玉林\AppData\Local\Temp\snapparse_*",
    "C:\Users\谢玉林\AppData\Local\Temp\import_vcvars_*",
    "C:\Users\谢玉林\AppData\Local\Temp\*ecopaste*"
)

foreach ($item in $cItems) {
    Get-Item $item | ForEach-Object {
        Write-Host "  Removed [C:]: $($_.FullName)" -ForegroundColor DarkGray
        Remove-Item -Recurse -Force $_.FullName
    }
}

# 2. Clean D: Drive temporary files
$dItems = @(
    "D:\play\SnapParse\src-qt\dist",
    "D:\play\SnapParse\src-qt\tools\launcher\payload.zip",
    "D:\play\SnapParse\src-qt\tools\launcher\launcher.res",
    "D:\play\SnapParse\src-qt\tools\launcher\app_icon.ico",
    "D:\play\SnapParse\src-qt\tools\launcher\main.obj",
    "D:\user\Temp\snapparse_*",
    "D:\user\Temp\import_vcvars_*",
    "D:\user\Temp\*ecopaste*"
)

foreach ($item in $dItems) {
    Get-Item $item | ForEach-Object {
        Write-Host "  Removed [D:]: $($_.FullName)" -ForegroundColor DarkGray
        Remove-Item -Recurse -Force $_.FullName
    }
}

Write-Host "Temporary files and build caches cleaned successfully!" -ForegroundColor Green
