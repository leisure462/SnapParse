#include "ClipboardMonitor.h"
#include "AppConfig.h"
#include "DatabaseManager.h"
#include "ContentParser.h"
#include "SensitiveDetector.h"
#include "SystemProcessManager.h"
#include "AppPaths.h"
#include "Logger.h"
#include "EventBus.h"

#include <QGuiApplication>
#include <QMimeData>
#include <QImage>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QSet>
#include <windows.h>
#include <psapi.h>

ClipboardMonitor* ClipboardMonitor::instance() {
    static ClipboardMonitor s_instance;
    return &s_instance;
}

ClipboardMonitor::ClipboardMonitor(QObject* parent) : QObject(parent) {
}

ClipboardMonitor::~ClipboardMonitor() {
    stop();
}

void ClipboardMonitor::start() {
    if (m_listening) return;
    m_listening = true;

    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &ClipboardMonitor::processClipboardChange);
    Logger::info("Clipboard listener started successfully via QClipboard");
}

void ClipboardMonitor::stop() {
    if (!m_listening) return;
    m_listening = false;

    disconnect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &ClipboardMonitor::processClipboardChange);
    Logger::info("Clipboard listener stopped");
}

ClipboardApp ClipboardMonitor::detectActiveApp() {
    ClipboardApp app;
    app.platform = "windows";

    HWND foreground = GetForegroundWindow();
    if (!foreground) return app;

    DWORD pid = 0;
    GetWindowThreadProcessId(foreground, &pid);
    if (pid == 0) return app;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess) {
        wchar_t exePath[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, exePath, &size)) {
            QString path = QString::fromWCharArray(exePath);
            QFileInfo fi(path);
            app.id = fi.fileName().toLower();
            app.name = SystemProcessManager::getAppFriendlyName(path, fi.baseName());
            app.iconFile = SystemProcessManager::extractAndCacheAppIcon(path, app.id);
        }
        CloseHandle(hProcess);
    }

    return app;
}

void ClipboardMonitor::processClipboardChange() {
    if (!m_listening) return;

    if (m_suppressNext) {
        m_suppressNext = false;
        return;
    }

    auto config = AppConfig::instance();
    ClipboardApp sourceApp = detectActiveApp();

    // Check app exclusion filter
    if (!sourceApp.id.isEmpty() && config->filters.excludedAppIds.contains(sourceApp.id, Qt::CaseInsensitive)) {
        Logger::info("Ignored clipboard copy from excluded app: " + sourceApp.id);
        return;
    }

    // Save app to DB
    if (!sourceApp.id.isEmpty()) {
        DatabaseManager::instance()->createOrUpdateApp(sourceApp);
    }

    const QClipboard* clipboard = QGuiApplication::clipboard();
    const QMimeData* mime = clipboard->mimeData();
    if (!mime) return;

    ClipboardItem item;
    item.platform = "windows";
    item.sourceAppId = sourceApp.id;

    // Follow Capture order
    bool captured = false;
    for (const QString& kind : config->capture.order) {
        if (kind == "files" && config->capture.files && mime->hasUrls()) {
            QList<QUrl> urls = mime->urls();
            QStringList paths;
            for (const QUrl& url : urls) {
                if (url.isLocalFile()) {
                    paths.append(url.toLocalFile());
                }
            }

            if (!paths.isEmpty()) {
                item.kind = "files";
                item.content = paths.join("\n");
                item.contentHash = ContentParser::computeHash(item.content);
                item.searchText = item.content;
                item.size = paths.size();

                if (paths.size() == 1) {
                    QFileInfo fi(paths.first());
                    if (fi.isDir()) {
                        item.subKind = "folder";
                        item.summary = fi.fileName().isEmpty() ? fi.absoluteFilePath() : fi.fileName();
                    } else {
                        QString ext = fi.suffix().toLower();
                        static const QSet<QString> imgExts = {"png", "jpg", "jpeg", "bmp", "gif", "webp", "ico", "svg"};
                        if (imgExts.contains(ext)) {
                            item.subKind = "image_file";
                        } else {
                            item.subKind = "file";
                        }
                        item.summary = fi.fileName();
                    }
                } else {
                    item.subKind = "files";
                    item.summary = QString("%1 个文件").arg(paths.size());
                }
                captured = true;
                break;
            }
        }
        else if (kind == "image" && config->capture.image && mime->hasImage()) {
            QImage img = qvariant_cast<QImage>(mime->imageData());
            if (!img.isNull()) {
                int w = 0, h = 0;
                qint64 sz = 0;
                QString imgFile = ContentParser::saveImageAndGetFilename(img, w, h, sz);
                
                // Max MB check
                if (config->capture.maxImageMb > 0 && sz > config->capture.maxImageMb * 1024 * 1024) {
                    return; // skip oversized
                }

                item.kind = "image";
                item.content = imgFile;
                item.contentHash = ContentParser::computeHash(imgFile);
                item.width = w;
                item.height = h;
                item.size = sz;
                item.summary = QString("%1 × %2 px").arg(w).arg(h);
                item.searchText = "image " + item.summary;
                captured = true;
                break;
            }
        }
        else if (kind == "html" && config->capture.html && mime->hasHtml()) {
            QString html = mime->html();
            QString plain = mime->text();
            qint64 sz = html.toUtf8().size();

            if (config->capture.maxTextMb > 0 && sz > config->capture.maxTextMb * 1024 * 1024) {
                return;
            }

            bool isSecret = SensitiveDetector::isSensitive(plain.isEmpty() ? html : plain);
            if (isSecret && !config->sensitive.collectSecrets) {
                Logger::info("Dropped sensitive secret copy from HTML per policy");
                return;
            }

            item.kind = "html";
            item.content = html;
            item.contentHash = ContentParser::computeHash(html);
            item.searchText = plain.isEmpty() ? html : plain;
            item.summary = (isSecret && config->sensitive.redactSecrets)
                ? SensitiveDetector::redact(ContentParser::generateSummary(item.searchText))
                : ContentParser::generateSummary(item.searchText);
            item.size = sz;
            item.subKind = ContentParser::detectSubKind(item.searchText);
            item.isSensitive = isSecret;
            captured = true;
            break;
        }
        else if (kind == "rtf" && config->capture.rtf && mime->hasFormat("text/rtf")) {
            QByteArray rtfData = mime->data("text/rtf");
            QString rtf = QString::fromUtf8(rtfData);
            QString plain = mime->text();
            qint64 sz = rtfData.size();

            if (config->capture.maxTextMb > 0 && sz > config->capture.maxTextMb * 1024 * 1024) {
                return;
            }

            bool isSecret = SensitiveDetector::isSensitive(plain.isEmpty() ? rtf : plain);
            if (isSecret && !config->sensitive.collectSecrets) {
                Logger::info("Dropped sensitive secret copy from RTF per policy");
                return;
            }

            item.kind = "rtf";
            item.content = rtf;
            item.contentHash = ContentParser::computeHash(rtf);
            item.searchText = plain.isEmpty() ? rtf : plain;
            item.summary = (isSecret && config->sensitive.redactSecrets)
                ? SensitiveDetector::redact(ContentParser::generateSummary(item.searchText))
                : ContentParser::generateSummary(item.searchText);
            item.size = sz;
            item.subKind = ContentParser::detectSubKind(item.searchText);
            item.isSensitive = isSecret;
            captured = true;
            break;
        }
        else if (kind == "text" && config->capture.text && mime->hasText()) {
            QString text = mime->text();
            if (text.trimmed().isEmpty()) continue;
            qint64 sz = text.toUtf8().size();

            if (config->capture.maxTextMb > 0 && sz > config->capture.maxTextMb * 1024 * 1024) {
                return;
            }

            // Sensitive check
            bool isSecret = SensitiveDetector::isSensitive(text);
            if (isSecret && !config->sensitive.collectSecrets) {
                Logger::info("Dropped sensitive secret copy per policy");
                return;
            }

            // Check if text is a single local file or folder path (e.g. file:///D:/... or D:/...)
            QString trimmedText = text.trimmed();
            QString localPath = trimmedText;
            if (localPath.startsWith("file:///", Qt::CaseInsensitive)) {
                localPath = QUrl(localPath).toLocalFile();
            } else if (localPath.startsWith("file://", Qt::CaseInsensitive)) {
                localPath = QUrl(localPath).toLocalFile();
            }
            if (!localPath.isEmpty() && QFileInfo::exists(localPath)) {
                QFileInfo fi(localPath);
                item.kind = "files";
                item.content = localPath;
                item.contentHash = ContentParser::computeHash(item.content);
                item.searchText = item.content;
                item.size = 1;
                if (fi.isDir()) {
                    item.subKind = "folder";
                    item.summary = fi.fileName().isEmpty() ? fi.absoluteFilePath() : fi.fileName();
                } else {
                    QString ext = fi.suffix().toLower();
                    static const QSet<QString> imgExts = {"png", "jpg", "jpeg", "bmp", "gif", "webp", "ico", "svg"};
                    if (imgExts.contains(ext)) {
                        item.subKind = "image_file";
                    } else {
                        item.subKind = "file";
                    }
                    item.summary = fi.fileName();
                }
                captured = true;
                break;
            }

            item.kind = "text";
            item.content = text;
            item.contentHash = ContentParser::computeHash(text);
            item.searchText = text;
            item.summary = (isSecret && config->sensitive.redactSecrets)
                ? SensitiveDetector::redact(ContentParser::generateSummary(text))
                : ContentParser::generateSummary(text);
            item.size = sz;
            item.subKind = ContentParser::detectSubKind(text);
            item.isSensitive = isSecret;
            captured = true;
            break;
        }
    }

    if (captured) {
        if (config->content.autoFavorite) {
            item.isFavorite = true;
        }

        DatabaseManager::instance()->insertItem(item);
        emit itemCaptured(item);
        EventBus::instance()->clipboardUpdated();
        EventBus::instance()->clipboardItemAdded(item.id);

        if (config->feedback.copySound) {
            MessageBeep(MB_OK);
        }
    }
}
