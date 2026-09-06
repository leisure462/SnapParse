#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <windows.h>
#include <ole2.h>

#include "AppPaths.h"
#include "Logger.h"
#include "AppConfig.h"
#include "DatabaseManager.h"
#include "ClipboardMonitor.h"
#include "GlobalHookManager.h"
#include "HotkeyManager.h"
#include "HistoryCleanService.h"
#include "MemoryManager.h"
#include "EventBus.h"
#include "ThemeManager.h"
#include "SystemTrayManager.h"
#include "PreferencesWindow.h"
#include "ClipboardWindow.h"
#include "PreviewWindow.h"
#include "FluentIcon.h"

static QString singleInstanceKey() {
    return QString("SnapParse_Qt_Server_v082_%1").arg(qEnvironmentVariable("USERNAME"));
}

LONG WINAPI CustomCrashHandler(EXCEPTION_POINTERS* pExceptionPointers) {
    DWORD code = pExceptionPointers->ExceptionRecord->ExceptionCode;
    PVOID addr = pExceptionPointers->ExceptionRecord->ExceptionAddress;

    wchar_t localApp[MAX_PATH];
    GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH);
    std::wstring crashPath = std::wstring(localApp) + L"\\EcoPasteHub\\EcoPaste\\logs\\crash.txt";

    HANDLE hFile = CreateFileW(
        crashPath.c_str(),
        GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    );
    if (hFile != INVALID_HANDLE_VALUE) {
        char buf[256];
        int len = sprintf_s(buf, "CRASH EXCEPTION: Code=0x%lX at Address=0x%p\n", code, addr);
        DWORD written = 0;
        WriteFile(hFile, buf, len, &written, NULL);
        CloseHandle(hFile);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char *argv[]) {
    OleInitialize(NULL);
    SetUnhandledExceptionFilter(CustomCrashHandler);

    QApplication app(argc, argv);
    app.setApplicationName("SnapParse");
    app.setApplicationVersion("3.0.0");
    app.setOrganizationName("SnapParseHub");
    app.setWindowIcon(FluentIcon::appIcon(64, true));
    app.setQuitOnLastWindowClosed(false); // Keep running in background/tray

    QFont defaultFont("Microsoft YaHei UI", 9);
    defaultFont.setStyleHint(QFont::SansSerif);
    defaultFont.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(defaultFont);

    AppPaths::init();
    Logger::init();
    MemoryManager::instance()->init();
    Logger::info("Starting SnapParse Qt main entry...");

    // 1. Single Instance Check via QLocalServer
    QString serverKey = singleInstanceKey();
    QLocalServer* localServer = new QLocalServer(&app);
    if (!localServer->listen(serverKey)) {
        QLocalSocket socket;
        socket.connectToServer(serverKey);
        if (socket.waitForConnected(200)) {
            Logger::info("Another instance is running, forwarding command and exiting");
            socket.write("show");
            socket.waitForBytesWritten(200);
            OleUninitialize();
            return 0;
        }
        // Stale server socket, remove and re-listen
        QLocalServer::removeServer(serverKey);
        localServer->listen(serverKey);
    }

    // 3. Core initialization
    Logger::info("Loading AppConfig...");
    AppConfig::instance()->load();

    Logger::info("Initializing DatabaseManager...");
    DatabaseManager::instance()->init();

    Logger::info("Applying theme...");
    ThemeManager::instance()->applyTheme();

    // 4. UI Windows created first
    Logger::info("Creating ClipboardWindow...");
    ClipboardWindow* clipboardWin = new ClipboardWindow();
    Logger::info("Creating PreferencesWindow...");
    PreferencesWindow* prefWin = new PreferencesWindow();
    Logger::info("Creating PreviewWindow...");
    PreviewWindow* previewWin = new PreviewWindow();

    Logger::info("Initializing SystemTrayManager...");
    SystemTrayManager::instance()->init();

    // 5. Services & Hooks started after UI windows ready
    Logger::info("Starting ClipboardMonitor...");
    ClipboardMonitor::instance()->start();
    Logger::info("Registering Hotkeys...");
    HotkeyManager::instance()->registerHotkeys();
    Logger::info("Starting GlobalHookManager...");
    GlobalHookManager::instance()->start();
    Logger::info("Starting HistoryCleanService...");
    HistoryCleanService::instance()->start();

    // 6. Handle single instance messages from new launches
    QObject::connect(localServer, &QLocalServer::newConnection, [localServer, clipboardWin, prefWin]() {
        QLocalSocket* client = localServer->nextPendingConnection();
        if (client) {
            QObject::connect(client, &QLocalSocket::readyRead, [client, clipboardWin, prefWin]() {
                QByteArray cmd = client->readAll();
                if (cmd.contains("pref")) {
                    prefWin->showAndFocus();
                } else {
                    clipboardWin->showAndFocus();
                }
            });
        }
    });

    // 7. Wire EventBus
    QObject::connect(EventBus::instance(), &EventBus::showClipboardWindow, [clipboardWin]() {
        clipboardWin->showAndFocus();
    });

    QObject::connect(EventBus::instance(), &EventBus::hideClipboardWindow, [clipboardWin]() {
        clipboardWin->hideWindow();
    });

    QObject::connect(EventBus::instance(), &EventBus::toggleClipboardWindow, [clipboardWin]() {
        if (clipboardWin->isVisible()) {
            clipboardWin->hideWindow();
        } else {
            clipboardWin->showAndFocus();
        }
    });

    QObject::connect(EventBus::instance(), &EventBus::showPreferencesWindow, [prefWin](const QString& tab, const QString& set) {
        prefWin->showAndFocus(tab, set);
    });

    QObject::connect(EventBus::instance(), &EventBus::showPreviewWindow, [previewWin](const QString& itemId, const QRect& anchorRect, const QRect& windowRect) {
        auto opt = DatabaseManager::instance()->getItem(itemId);
        if (opt) {
            previewWin->showPreview(*opt, anchorRect, windowRect);
        }
    });

    QObject::connect(EventBus::instance(), &EventBus::hidePreviewWindow, [previewWin]() {
        previewWin->hide();
    });

    // 8. Launch policy: If manual start (not --autostart), popup ClipboardWindow immediately
    bool isAutostart = false;
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--autostart") {
            isAutostart = true;
            break;
        }
    }

    if (!isAutostart) {
        clipboardWin->showAndFocus();
    }

    Logger::info("SnapParse Qt initialized successfully");

    int exitCode = app.exec();
    OleUninitialize();
    return exitCode;
}
