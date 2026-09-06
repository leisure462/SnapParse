#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <strsafe.h>
#include <string>

// Resource ID for embedded payload zip
#define IDR_PAYLOAD_ZIP 101

// Version identifier: changing this will trigger re-extraction on update
#define APP_VERSION_TAG L"v3.0.0"

static std::wstring GetLocalAppDataPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
        return std::wstring(path);
    }
    GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH);
    return std::wstring(path);
}

static std::wstring GetTempPathStr() {
    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    return std::wstring(path);
}

static bool DirectoryExists(const std::wstring& path) {
    DWORD dwAttrib = GetFileAttributesW(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

static bool FileExists(const std::wstring& path) {
    DWORD dwAttrib = GetFileAttributesW(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

static bool CreateDirectoryRecursive(const std::wstring& path) {
    if (DirectoryExists(path)) return true;
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        CreateDirectoryRecursive(path.substr(0, pos));
    }
    return CreateDirectoryW(path.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static std::wstring GetShortPath(const std::wstring& path) {
    wchar_t shortBuf[MAX_PATH];
    DWORD len = GetShortPathNameW(path.c_str(), shortBuf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return std::wstring(shortBuf);
    }
    return path;
}

static bool ExtractEmbeddedZip(const std::wstring& targetZipPath) {
    HMODULE hModule = GetModuleHandleW(NULL);
    HRSRC hRes = FindResourceW(hModule, MAKEINTRESOURCEW(IDR_PAYLOAD_ZIP), RT_RCDATA);
    if (!hRes) return false;

    HGLOBAL hMem = LoadResource(hModule, hRes);
    if (!hMem) return false;

    DWORD size = SizeofResource(hModule, hRes);
    const void* data = LockResource(hMem);
    if (!data || size == 0) return false;

    HANDLE hFile = CreateFileW(
        targetZipPath.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, data, size, &written, NULL);
    CloseHandle(hFile);

    return ok && (written == size);
}

static bool RunCommandHidden(const std::wstring& cmd) {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::wstring cmdCopy = cmd;
    if (!CreateProcessW(NULL, &cmdCopy[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return false;
    }

    WaitForSingleObject(pi.hProcess, 60000); // Max 60 seconds wait
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return (exitCode == 0);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)pCmdLine;
    (void)nCmdShow;

    std::wstring localAppData = GetLocalAppDataPath();
    std::wstring runtimeDir = localAppData + L"\\SnapParseHub\\SnapParse\\runtime-" + APP_VERSION_TAG;
    std::wstring mainExePath = runtimeDir + L"\\SnapParse.exe";

    // 1. Check if the runtime is already extracted and valid
    if (!FileExists(mainExePath)) {
        CreateDirectoryRecursive(runtimeDir);

        std::wstring tempZip = GetTempPathStr() + L"snapparse_payload.zip";

        if (!ExtractEmbeddedZip(tempZip)) {
            MessageBoxW(NULL, L"无法提取程序运行环境，请检查磁盘空间与权限。", L"SnapParse 错误", MB_OK | MB_ICONERROR);
            return 1;
        }

        // Convert to 8.3 short paths for bulletproof Unicode/Chinese username support
        std::wstring shortZip = GetShortPath(tempZip);
        std::wstring shortRuntime = GetShortPath(runtimeDir);
        
        std::wstring tarCmd = L"\"C:\\Windows\\System32\\tar.exe\" -xf \"" + shortZip + L"\" -C \"" + shortRuntime + L"\"";
        RunCommandHidden(tarCmd);

        DeleteFileW(tempZip.c_str());

        if (!FileExists(mainExePath)) {
            MessageBoxW(NULL, L"初始化程序文件失败，请尝试以管理员身份运行。", L"SnapParse 错误", MB_OK | MB_ICONERROR);
            return 1;
        }
    }

    // 2. Launch the target main application passing all original command line arguments
    std::wstring cmdLineToPass = L"\"" + mainExePath + L"\"";
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            cmdLineToPass += L" \"";
            cmdLineToPass += argv[i];
            cmdLineToPass += L"\"";
        }
        LocalFree(argv);
    }

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW;

    // Launch in the runtime directory so DLLs and plugins load with zero delay
    BOOL launched = CreateProcessW(
        mainExePath.c_str(),
        &cmdLineToPass[0],
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        runtimeDir.c_str(),
        &si,
        &pi
    );

    if (launched) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 0;
    } else {
        DWORD err = GetLastError();
        wchar_t errMsg[256];
        StringCchPrintfW(errMsg, 256, L"启动 SnapParse 失败 (错误码: %lu)。", err);
        MessageBoxW(NULL, errMsg, L"SnapParse 启动错误", MB_OK | MB_ICONERROR);
        return 1;
    }
}
