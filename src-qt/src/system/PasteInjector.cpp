#include "PasteInjector.h"
#include "ClipboardMonitor.h"
#include "AppPaths.h"
#include "Logger.h"
#include "EventBus.h"

#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
#include <QUrl>
#include <QImage>
#include <QThread>

PasteInjector* PasteInjector::instance() {
    static PasteInjector s_instance;
    return &s_instance;
}

void PasteInjector::recordTargetWindow() {
    HWND fg = GetForegroundWindow();
    if (fg) {
        m_targetWindow = fg;
    }
}

void PasteInjector::writeToClipboard(const ClipboardItem& item, bool plain, bool filesAsPath) {
    ClipboardMonitor::instance()->setSuppressNext(true);

    QClipboard* cb = QGuiApplication::clipboard();
    QMimeData* mime = new QMimeData();

    if (item.kind == "files") {
        if (filesAsPath) {
            mime->setText(item.content);
        } else {
            QStringList paths = item.content.split('\n');
            QList<QUrl> urls;
            for (const QString& p : paths) {
                QString path = p.trimmed();
                if (path.startsWith("file:///", Qt::CaseInsensitive) || path.startsWith("file://", Qt::CaseInsensitive)) {
                    path = QUrl(path).toLocalFile();
                }
                if (!path.isEmpty()) {
                    urls.append(QUrl::fromLocalFile(path));
                }
            }
            if (!urls.isEmpty()) {
                mime->setUrls(urls);
            }
            mime->setText(item.content);
        }
    }
    else if (item.kind == "image") {
        QString imgPath = AppPaths::imageCacheDir() + "/" + item.content;
        QImage img(imgPath);
        if (!img.isNull()) {
            mime->setImageData(img);
        }
    }
    else if (item.kind == "html") {
        if (plain) {
            mime->setText(item.searchText.isEmpty() ? item.content : item.searchText);
        } else {
            mime->setHtml(item.content);
            mime->setText(item.searchText);
        }
    }
    else {
        mime->setText(item.content);
    }

    cb->setMimeData(mime);
}

void PasteInjector::pasteItem(const ClipboardItem& item, bool plain, bool filesAsPath) {
    // 1. Write to clipboard
    writeToClipboard(item, plain, filesAsPath);

    // 2. Hide clipboard window
    EventBus::instance()->hideClipboardWindow();

    // 3. Restore target window focus
    if (m_targetWindow && IsWindow(m_targetWindow)) {
        SetForegroundWindow(m_targetWindow);
    }

    // 4. Slight delay to ensure focus switch before key injection
    QThread::msleep(40);

    // 5. Send Ctrl + V keystrokes
    INPUT inputs[4] = {};

    // Ctrl Down
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;

    // V Down
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';

    // V Up
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

    // Ctrl Up
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(4, inputs, sizeof(INPUT));
}
