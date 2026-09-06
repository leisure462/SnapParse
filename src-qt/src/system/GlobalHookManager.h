#pragma once

#include <QObject>
#include <QRect>
#include <windows.h>

class GlobalHookManager : public QObject {
    Q_OBJECT
public:
    static GlobalHookManager* instance();

    void start();
    void stop();
    void setClipboardWindowHandle(HWND hwnd);
    void setPinned(bool pinned);
    static void markWindowShown();
    static QString getForegroundProcessName();

signals:
    void clickOutside();
    void winVTriggered();

private:
    explicit GlobalHookManager(QObject* parent = nullptr);
    ~GlobalHookManager();

    static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

    static HHOOK s_mouseHook;
    static HHOOK s_keyboardHook;
    static HWND s_clipboardHwnd;
    static bool s_isPinned;
    static qint64 s_lastShowTime;
};
