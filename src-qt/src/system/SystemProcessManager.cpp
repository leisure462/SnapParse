#include "SystemProcessManager.h"
#include "AppPaths.h"
#include "Logger.h"

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shellapi.h>
#include <winver.h>

#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QPixmap>
#include <QSet>
#include <QMap>
#include <algorithm>

static bool isSystemInternalProcess(const QString& exeLower) {
    static const QSet<QString> systemExes = {
        "system", "idle", "registry", "smss.exe", "csrss.exe", "wininit.exe",
        "services.exe", "lsass.exe", "svchost.exe", "fontdrvhost.exe", "dwm.exe",
        "winlogon.exe", "sihost.exe", "ctfmon.exe", "conhost.exe", "spoolsv.exe",
        "dashost.exe", "securityhealthservice.exe", "audiodg.exe", "wlanext.exe",
        "searchhost.exe", "startmenuexperiencehost.exe", "searchapp.exe",
        "shellexperiencehost.exe", "taskhostw.exe", "dllhost.exe", "runtimebroker.exe",
        "searchindexer.exe", "msmpeng.exe", "smartscreen.exe", "widgetservice.exe",
        "backgroundtaskhost.exe", "compattelrunner.exe", "wudfhost.exe", "wmiprvse.exe",
        "svchost", "system idle process", "memory compression"
    };
    return systemExes.contains(exeLower);
}

QString SystemProcessManager::getAppFriendlyName(const QString& exePath, const QString& defaultName) {
    if (exePath.isEmpty()) return defaultName;

    DWORD dummy = 0;
    DWORD size = GetFileVersionInfoSizeW((LPCWSTR)exePath.utf16(), &dummy);
    if (size > 0) {
        QByteArray data(size, 0);
        if (GetFileVersionInfoW((LPCWSTR)exePath.utf16(), 0, size, data.data())) {
            // Read Translation CodePage
            struct LANGANDCODEPAGE {
                WORD wLanguage;
                WORD wCodePage;
            } *lpTranslate = nullptr;
            UINT cbTranslate = 0;

            if (VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", (LPVOID*)&lpTranslate, &cbTranslate) && cbTranslate >= sizeof(LANGANDCODEPAGE)) {
                WCHAR subBlock[128];
                wsprintfW(subBlock, L"\\StringFileInfo\\%04x%04x\\FileDescription", lpTranslate[0].wLanguage, lpTranslate[0].wCodePage);
                LPVOID lpBuffer = nullptr;
                UINT dwBytes = 0;
                if (VerQueryValueW(data.data(), subBlock, &lpBuffer, &dwBytes) && dwBytes > 0 && lpBuffer != nullptr) {
                    QString desc = QString::fromWCharArray((LPCWSTR)lpBuffer).trimmed();
                    if (!desc.isEmpty()) return desc;
                }
            }

            // Fallback to standard 040904b0 (English) or 080404b0 (Chinese)
            for (const wchar_t* codePage : {L"040904b0", L"080404b0", L"040904E4", L"080404E4", L"000004b0"}) {
                WCHAR subBlock[128];
                wsprintfW(subBlock, L"\\StringFileInfo\\%s\\FileDescription", codePage);
                LPVOID lpBuffer = nullptr;
                UINT dwBytes = 0;
                if (VerQueryValueW(data.data(), subBlock, &lpBuffer, &dwBytes) && dwBytes > 0 && lpBuffer != nullptr) {
                    QString desc = QString::fromWCharArray((LPCWSTR)lpBuffer).trimmed();
                    if (!desc.isEmpty()) return desc;
                }
            }
        }
    }
    return defaultName;
}

QString SystemProcessManager::extractAndCacheAppIcon(const QString& exePath, const QString& appId) {
    if (exePath.isEmpty() || appId.isEmpty()) return "";

    QString iconFileName = appId + ".png";
    QString fullCachePath = AppPaths::iconCacheDir() + "/" + iconFileName;

    if (QFile::exists(fullCachePath)) {
        return iconFileName;
    }

    SHFILEINFOW sfi = {0};
    DWORD_PTR res = SHGetFileInfoW((LPCWSTR)exePath.utf16(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON);
    if (res && sfi.hIcon) {
        QPixmap pixmap = QPixmap::fromImage(QImage::fromHICON(sfi.hIcon));
        DestroyIcon(sfi.hIcon);
        if (!pixmap.isNull()) {
            QDir().mkpath(AppPaths::iconCacheDir());
            pixmap.save(fullCachePath, "PNG");
            return iconFileName;
        }
    }
    return "";
}

struct WindowEnumContext {
    QSet<DWORD> pids;
    QMap<DWORD, QString> windowTitles;
};

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindowTextLengthW(hwnd) == 0) return TRUE;

    LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return TRUE;

    RECT r;
    if (!GetWindowRect(hwnd, &r) || (r.right - r.left < 50) || (r.bottom - r.top < 50)) return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != 0) {
        WindowEnumContext* ctx = reinterpret_cast<WindowEnumContext*>(lParam);
        ctx->pids.insert(pid);

        WCHAR title[256];
        if (GetWindowTextW(hwnd, title, 256) > 0) {
            QString t = QString::fromWCharArray(title).trimmed();
            if (!t.isEmpty() && !ctx->windowTitles.contains(pid)) {
                ctx->windowTitles.insert(pid, t);
            }
        }
    }
    return TRUE;
}

QList<ClipboardApp> SystemProcessManager::listRunningApps() {
    QMap<QString, ClipboardApp> appMap;

    // 1. Enumerate visible desktop windows to prioritize active GUI apps
    WindowEnumContext winCtx;
    EnumWindows(EnumWindowsProc, (LPARAM)&winCtx);

    // 2. Snapshot all running processes
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        return {};
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnap, &pe32)) {
        do {
            QString exeFile = QString::fromWCharArray(pe32.szExeFile).trimmed();
            QString exeLower = exeFile.toLower();

            if (exeLower.isEmpty() || isSystemInternalProcess(exeLower)) {
                continue;
            }

            // Exclude current app process to avoid self-blocking
            if (exeLower == "snapparse.exe" || exeLower == "ecopaste.exe") {
                continue;
            }

            HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe32.th32ProcessID);
            QString fullExePath;
            if (hProcess) {
                WCHAR fullPath[MAX_PATH];
                DWORD size = MAX_PATH;
                if (QueryFullProcessImageNameW(hProcess, 0, fullPath, &size)) {
                    fullExePath = QString::fromWCharArray(fullPath);
                }
                CloseHandle(hProcess);
            }

            if (appMap.contains(exeLower)) {
                continue;
            }

            ClipboardApp app;
            app.id = exeLower;
            app.platform = "windows";

            QFileInfo fi(fullExePath.isEmpty() ? exeFile : fullExePath);
            QString baseName = fi.completeBaseName();

            // Friendly name from FileVersionInfo -> Window Title -> Base Executable Name
            QString friendlyName = getAppFriendlyName(fullExePath, "");
            if (friendlyName.isEmpty() && winCtx.windowTitles.contains(pe32.th32ProcessID)) {
                friendlyName = winCtx.windowTitles[pe32.th32ProcessID];
            }
            if (friendlyName.isEmpty()) {
                friendlyName = baseName;
            }
            app.name = friendlyName;

            // Extract real icon
            if (!fullExePath.isEmpty()) {
                app.iconFile = extractAndCacheAppIcon(fullExePath, exeLower);
            }

            appMap.insert(exeLower, app);

        } while (Process32NextW(hSnap, &pe32));
    }
    CloseHandle(hSnap);

    QList<ClipboardApp> result = appMap.values();

    // Sort apps alphabetically by name
    std::sort(result.begin(), result.end(), [](const ClipboardApp& a, const ClipboardApp& b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });

    return result;
}
