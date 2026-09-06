#include "GlobalHookManager.h"
#include "AppConfig.h"
#include "EventBus.h"
#include "Logger.h"
#include <QDateTime>
#include <QFileInfo>

HHOOK GlobalHookManager::s_mouseHook = nullptr;
HHOOK GlobalHookManager::s_keyboardHook = nullptr;
HWND GlobalHookManager::s_clipboardHwnd = nullptr;
bool GlobalHookManager::s_isPinned = false;
qint64 GlobalHookManager::s_lastShowTime = 0;

GlobalHookManager* GlobalHookManager::instance() {
    static GlobalHookManager s_instance;
    return &s_instance;
}

GlobalHookManager::GlobalHookManager(QObject* parent) : QObject(parent) {
}

GlobalHookManager::~GlobalHookManager() {
    stop();
}

void GlobalHookManager::setClipboardWindowHandle(HWND hwnd) {
    s_clipboardHwnd = hwnd;
}

void GlobalHookManager::setPinned(bool pinned) {
    s_isPinned = pinned;
}

void GlobalHookManager::markWindowShown() {
    s_lastShowTime = QDateTime::currentMSecsSinceEpoch();
}

QString GlobalHookManager::getForegroundProcessName() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return "";

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return "";

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return "";

    wchar_t exePath[MAX_PATH];
    DWORD size = MAX_PATH;
    QString procName;
    if (QueryFullProcessImageNameW(hProc, 0, exePath, &size)) {
        procName = QFileInfo(QString::fromWCharArray(exePath)).fileName();
    }
    CloseHandle(hProc);
    return procName;
}

void GlobalHookManager::start() {
    if (!s_mouseHook) {
        s_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, GetModuleHandle(nullptr), 0);
        if (!s_mouseHook) {
            Logger::error("Failed to install mouse hook");
        }
    }

    if (!s_keyboardHook) {
        s_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(nullptr), 0);
        if (!s_keyboardHook) {
            Logger::error("Failed to install keyboard hook");
        }
    }
}

void GlobalHookManager::stop() {
    if (s_mouseHook) {
        UnhookWindowsHookEx(s_mouseHook);
        s_mouseHook = nullptr;
    }
    if (s_keyboardHook) {
        UnhookWindowsHookEx(s_keyboardHook);
        s_keyboardHook = nullptr;
    }
}

LRESULT CALLBACK GlobalHookManager::MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (s_isPinned) {
        return CallNextHookEx(s_mouseHook, nCode, wParam, lParam);
    }
    if (nCode >= 0 && s_clipboardHwnd && IsWindowVisible(s_clipboardHwnd)) {
        // If shown within last 400ms, ignore clicks to avoid double-click dismissal race condition
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - s_lastShowTime > 400) {
            if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_NCLBUTTONDOWN) {
                MSLLHOOKSTRUCT* mouseStruct = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
                if (mouseStruct) {
                    POINT pt = mouseStruct->pt;
                    HWND clickedHwnd = WindowFromPoint(pt);
                    if (clickedHwnd != s_clipboardHwnd && !IsChild(s_clipboardHwnd, clickedHwnd)) {
                        RECT rc;
                        GetWindowRect(s_clipboardHwnd, &rc);
                        if (!PtInRect(&rc, pt)) {
                            // Clicked outside, hide clipboard window
                            EventBus::instance()->hideClipboardWindow();
                        }
                    }
                }
            }
        }
    }

    return CallNextHookEx(s_mouseHook, nCode, wParam, lParam);
}

LRESULT CALLBACK GlobalHookManager::KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* kbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (kbd && AppConfig::instance()->shortcuts.winV) {
            bool isWinDown = (GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000);
            if (isWinDown && kbd->vkCode == 'V' && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
                // Intercept Win+V
                EventBus::instance()->toggleClipboardWindow();
                return 1; // suppress Windows system clipboard panel
            }
        }
    }

    return CallNextHookEx(s_keyboardHook, nCode, wParam, lParam);
}
