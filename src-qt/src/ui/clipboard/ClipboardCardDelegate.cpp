#include "ClipboardCardDelegate.h"
#include "AppPaths.h"
#include "AppConfig.h"
#include "ThemeManager.h"
#include "SensitiveDetector.h"
#include "FluentIcon.h"
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QCache>
#include <QImageReader>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QGuiApplication>

static QCache<QString, QPixmap> s_thumbnailCache(150);
static QCache<QString, QPixmap> s_appIconCache(100);

static QFont getTextFont() {
    QFont f = QGuiApplication::font();
    f.setPointSize(9);
    return f;
}
static QFontMetrics getTextFm() {
    return QFontMetrics(getTextFont());
}
static QFont getDemiBoldFont() {
    QFont f = QGuiApplication::font();
    f.setPointSize(9);
    f.setWeight(QFont::DemiBold);
    return f;
}
static QFont getSmallFont() {
    QFont f = QGuiApplication::font();
    f.setPointSize(8);
    return f;
}

static bool isImageExtension(const QString& ext) {
    static const QSet<QString> imgExts = {"png", "jpg", "jpeg", "bmp", "gif", "webp", "ico", "svg"};
    return imgExts.contains(ext.toLower());
}

static bool isSingleImageFilePath(const QString& content) {
    QString path = content.trimmed();
    if (path.startsWith("file:///", Qt::CaseInsensitive)) {
        path = QUrl(path).toLocalFile();
    }
    if (path.contains('\n')) return false;
    QFileInfo fi(path);
    return isImageExtension(fi.suffix());
}

static QString getCleanFilePath(const QString& raw) {
    QString p = raw.trimmed();
    if (p.startsWith("file:///", Qt::CaseInsensitive) || p.startsWith("file://", Qt::CaseInsensitive)) {
        p = QUrl(p).toLocalFile();
    }
    return p;
}

static QString getSpecificTypeLabel(const QString& kind, const QString& subKind, const QString& content, qint64 size) {
    if (kind == "image") {
        return "图片";
    }
    if (kind == "html") {
        return "网页内容";
    }
    if (kind == "rtf") {
        return "富文本";
    }
    if (kind == "text") {
        if (subKind == "url") return "网页链接";
        if (subKind == "color") return "颜色代码";
        if (subKind == "email") return "电子邮箱";
        return "纯文本";
    }
    if (kind == "files") {
        QString cleanPath = getCleanFilePath(content);
        if (subKind == "folder" || QDir(cleanPath).exists()) {
            return "文件夹";
        }
        if (subKind == "image_file" || isSingleImageFilePath(cleanPath)) {
            QString ext = QFileInfo(cleanPath).suffix().toUpper();
            return ext.isEmpty() ? "图片文件" : QString("%1 图片").arg(ext);
        }
        if (cleanPath.contains('\n')) {
            int count = cleanPath.split('\n', Qt::SkipEmptyParts).size();
            return QString("文件 (%1 项)").arg(count);
        }
        if (size > 1) {
            return QString("文件 (%1 项)").arg(size);
        }
        QString ext = QFileInfo(cleanPath).suffix().toUpper();
        if (!ext.isEmpty()) {
            if (ext == "ZIP" || ext == "RAR" || ext == "7Z" || ext == "TAR" || ext == "GZ") {
                return QString("%1 压缩包").arg(ext);
            }
            return QString("%1 文件").arg(ext);
        }
        return "文件";
    }
    return "条目";
}

ClipboardCardDelegate::ClipboardCardDelegate(QObject* parent) : QStyledItemDelegate(parent) {
}

void ClipboardCardDelegate::clearThumbnailCache() {
    s_thumbnailCache.clear();
    s_appIconCache.clear();
}

QSize ClipboardCardDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(option)
    auto cfg = AppConfig::instance();
    QString kind = index.data(Qt::UserRole + 1).toString();
    QString content = index.data(Qt::UserRole + 2).toString();
    QString subKind = index.data(Qt::UserRole + 12).toString();

    if (kind == "image" || (kind == "files" && (subKind == "image_file" || isSingleImageFilePath(content)))) {
        int imgH = qBound(32, cfg->display.imageMaxHeight, 80);
        return QSize(340, 48 + imgH);
    } else if (kind == "files") {
        QString cleanPath = getCleanFilePath(content);
        if (subKind == "folder" || QDir(cleanPath).exists() || !cleanPath.contains('\n')) {
            return QSize(340, 68);
        }
        int fileCount = cleanPath.split('\n', Qt::SkipEmptyParts).size();
        fileCount = qBound(1, fileCount, cfg->display.fileMaxCount);
        return QSize(340, 36 + fileCount * 20);
    }

    // Text / HTML / RTF / URL
    int maxAllowedLines = qBound(1, cfg->display.textMaxLines, 5);
    QString summary = index.data(Qt::UserRole + 3).toString();
    QString note = index.data(Qt::UserRole + 8).toString();
    QString displayText = note.isEmpty() ? summary : QString("%1\n%2").arg(note, summary);

    int lineH = getTextFm().lineSpacing();

    int availableWidth = 340 - 16 - 28;
    QRect bound = getTextFm().boundingRect(QRect(0, 0, availableWidth, 2000), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, displayText);
    int actualLines = qMax(1, (bound.height() + lineH / 2) / lineH);
    int displayLines = qMin(actualLines, maxAllowedLines);

    return QSize(340, 48 + displayLines * lineH);
}

void ClipboardCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    auto cfg = AppConfig::instance();
    // Left margin 8px, right margin inside viewport 4px (plus 4px scrollbar = exactly 8px right distance from window border)
    QRect rect = option.rect.adjusted(8, 3, -4, -3);
    bool isSelected = option.state & QStyle::State_Selected;
    bool isHover = option.state & QStyle::State_MouseOver;

    // Card background path (10px modern rounded rect)
    QPainterPath path;
    path.addRoundedRect(rect, 10, 10);

    QColor cardBg = ThemeManager::instance()->cardGlassColor();
    if (isSelected) {
        cardBg = ThemeManager::instance()->cardGlassSelectedColor();
    } else if (isHover) {
        cardBg = ThemeManager::instance()->cardGlassHoverColor();
    }
    painter->fillPath(path, cardBg);

    // Card border: 1px with soft alpha for a refined feel
    QColor p = ThemeManager::instance()->primaryColor();
    bool dark = ThemeManager::instance()->isDarkMode();
    QColor borderCol = isSelected 
        ? QColor(p.red(), p.green(), p.blue(), dark ? 100 : 80) 
        : ThemeManager::instance()->glassBorderColor();
    borderCol.setAlpha(qMax(1, borderCol.alpha() * 2 / 3)); // softer than raw to compensate for 1px vs 0.5px
    painter->setPen(QPen(borderCol, 1));
    painter->drawPath(path);

    // Data values
    QString kind = index.data(Qt::UserRole + 1).toString();
    QString content = index.data(Qt::UserRole + 2).toString();
    QString summary = index.data(Qt::UserRole + 3).toString();
    QString appName = index.data(Qt::UserRole + 4).toString();
    QString createdAt = index.data(Qt::UserRole + 5).toString();
    bool isFav = index.data(Qt::UserRole + 6).toBool();
    bool isPin = index.data(Qt::UserRole + 7).toBool();
    QString note = index.data(Qt::UserRole + 8).toString();
    bool isSensitive = index.data(Qt::UserRole + 9).toBool();
    QString sourceAppId = index.data(Qt::UserRole + 10).toString();
    QString sourceAppIcon = index.data(Qt::UserRole + 11).toString();
    QString subKind = index.data(Qt::UserRole + 12).toString();
    qint64 itemSize = index.data(Qt::UserRole + 13).toLongLong();

    if (isSensitive && cfg->sensitive.redactSecrets) {
        summary = SensitiveDetector::redact(summary);
    }

    // Top Row: [App Icon] [Type Label] Left, (Time) Right
    int topY = rect.top() + 7;
    int leftX = rect.left() + 10;

    // 1. Draw App Icon (15x15)
    QRect appIconRect(leftX, topY, 15, 15);
    QPixmap* appPix = nullptr;
    QString iconKey = sourceAppId.isEmpty() ? "default_app" : sourceAppId;
    appPix = s_appIconCache.object(iconKey);

    if (!appPix) {
        QString iconFile = sourceAppIcon.isEmpty() ? (sourceAppId + ".png") : sourceAppIcon;
        QString iconPath = AppPaths::iconCacheDir() + "/" + iconFile;
        if (QFile::exists(iconPath)) {
            QPixmap loaded(iconPath);
            if (!loaded.isNull()) {
                appPix = new QPixmap(loaded.scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                s_appIconCache.insert(iconKey, appPix);
            }
        }
    }

    if (appPix && !appPix->isNull()) {
        painter->drawPixmap(appIconRect, *appPix);
    } else {
        // Fallback default clean vector app icon
        QColor defIconCol = dark ? QColor("#a1a1a6") : QColor("#86868b");
        QPixmap defPix = FluentIcon::make(FluentIconType::Workflow, defIconCol, 14).pixmap(15, 15);
        painter->drawPixmap(appIconRect, defPix);
    }

    // 2. Draw Type Label (next to app icon)
    QString typeLabel = getSpecificTypeLabel(kind, subKind, content, itemSize);
    QString badge;
    if (isSensitive && cfg->sensitive.redactSecrets) badge += "🔒 ";
    if (isPin) badge += "📌 ";
    if (isFav) badge += "⭐ ";

    painter->setFont(getDemiBoldFont());
    QColor typeColor = isSelected ? ThemeManager::instance()->primaryColor() : (dark ? QColor("#b0b0b8") : QColor("#5a5a60"));
    painter->setPen(typeColor);
    painter->drawText(leftX + 20, topY + 12, badge + typeLabel);

    // Time (Relative friendly format) - hidden when hovered or selected to avoid overlapping with quick action bar
    if (!isSelected && !isHover) {
        painter->setFont(getSmallFont());
        painter->setPen(ThemeManager::instance()->secondaryTextColor());
        QString timeStr = createdAt.length() >= 19 ? createdAt.mid(11, 5) : "";
        painter->drawText(rect.right() - 48, topY + 12, timeStr);
    }

    // Content Body
    int contentY = rect.top() + 27;
    painter->setFont(getTextFont());
    painter->setPen(ThemeManager::instance()->textColor());

    bool isImageCard = (kind == "image") || (kind == "files" && (subKind == "image_file" || isSingleImageFilePath(content)));

    if (isImageCard) {
        int imgH = qBound(32, cfg->display.imageMaxHeight, 80);
        int imgW = qBound(48, static_cast<int>(imgH * 1.5), 100);
        QRect imgRect(rect.left() + 10, contentY, imgW, imgH);

        QString imgSourcePath;
        if (kind == "image") {
            imgSourcePath = AppPaths::imageCacheDir() + "/" + content;
        } else {
            imgSourcePath = getCleanFilePath(content);
        }

        QString cacheKey = QString("%1_%2x%3").arg(imgSourcePath).arg(imgW).arg(imgH);
        QPixmap* cached = s_thumbnailCache.object(cacheKey);

        if (!cached) {
            if (QFile::exists(imgSourcePath)) {
                QImageReader reader(imgSourcePath);
                reader.setAutoTransform(true);
                QSize origSize = reader.size();
                if (origSize.isValid() && !origSize.isEmpty()) {
                    const qreal dpr = qApp->devicePixelRatio();
                    QSize scaledSize = origSize.scaled(imgRect.size() * dpr, Qt::KeepAspectRatioByExpanding);
                    reader.setScaledSize(scaledSize);
                    QImage img = reader.read();
                    if (!img.isNull()) {
                        QPixmap pix(imgRect.size() * dpr);
                        pix.setDevicePixelRatio(dpr);
                        pix.fill(Qt::transparent);
                        QPainter p(&pix);
                        p.setRenderHint(QPainter::Antialiasing);
                        p.setRenderHint(QPainter::SmoothPixmapTransform);
                        QPainterPath clip;
                        clip.addRoundedRect(QRect(0, 0, pix.width(), pix.height()), 8 * dpr, 8 * dpr);
                        p.setClipPath(clip);
                        int drawX = (pix.width() - img.width()) / 2;
                        int drawY = (pix.height() - img.height()) / 2;
                        p.drawImage(drawX, drawY, img);
                        p.end();

                        cached = new QPixmap(pix);
                        s_thumbnailCache.insert(cacheKey, cached);
                    }
                }
            }
        }

        if (cached && !cached->isNull()) {
            painter->drawPixmap(imgRect, *cached);
            painter->setPen(QPen(ThemeManager::instance()->glassBorderColor(), 1));
            painter->drawRoundedRect(imgRect, 4, 4);

            QRect metaRect(imgRect.right() + 12, contentY, rect.right() - imgRect.right() - 20, imgH);
            painter->setFont(getTextFont());
            painter->setPen(ThemeManager::instance()->secondaryTextColor());

            QString metaText = summary;
            if (kind == "files") {
                QFileInfo fi(imgSourcePath);
                metaText = fi.fileName() + (summary.isEmpty() ? "" : ("\n" + summary));
            }
            painter->drawText(metaRect, Qt::AlignLeft | Qt::AlignVCenter, metaText);
        } else {
            // If image failed to load, display filename
            painter->drawText(leftX, contentY + 16, "🖼️ " + QFileInfo(imgSourcePath).fileName());
        }
    } else if (kind == "files") {
        QString cleanPath = getCleanFilePath(content);
        if (subKind == "folder" || QDir(cleanPath).exists()) {
            // Folder Card layout
            QFileInfo fi(cleanPath);
            QString folderName = fi.fileName().isEmpty() ? fi.absoluteFilePath() : fi.fileName();
            
            // Draw vector folder icon
            QRect folderIconRect(leftX, contentY + 2, 22, 22);
            QPixmap folderPix = FluentIcon::make(FluentIconType::Folder, ThemeManager::instance()->primaryColor(), 20).pixmap(22, 22);
            painter->drawPixmap(folderIconRect, folderPix);

            // Folder Name
            painter->setFont(getDemiBoldFont());
            painter->setPen(ThemeManager::instance()->textColor());
            painter->drawText(leftX + 28, contentY + 12, folderName);

            // Folder Path
            painter->setFont(getSmallFont());
            painter->setPen(ThemeManager::instance()->secondaryTextColor());
            painter->drawText(leftX + 28, contentY + 26, cleanPath);
        } else if (!cleanPath.contains('\n')) {
            // Single File Card layout
            QFileInfo fi(cleanPath);
            QString fileName = fi.fileName().isEmpty() ? cleanPath : fi.fileName();

            // Draw file icon
            QRect fileIconRect(leftX, contentY + 2, 22, 22);
            QPixmap filePix = FluentIcon::make(FluentIconType::FileText, ThemeManager::instance()->secondaryTextColor(), 20).pixmap(22, 22);
            painter->drawPixmap(fileIconRect, filePix);

            // File Name
            painter->setFont(getDemiBoldFont());
            painter->setPen(ThemeManager::instance()->textColor());
            painter->drawText(leftX + 28, contentY + 12, fileName);

            // File Path
            painter->setFont(getSmallFont());
            painter->setPen(ThemeManager::instance()->secondaryTextColor());
            painter->drawText(leftX + 28, contentY + 26, cleanPath);
        } else {
            // Multi files list
            int maxFiles = qBound(1, cfg->display.fileMaxCount, 5);
            QStringList fileLines = cleanPath.split('\n', Qt::SkipEmptyParts);
            int drawn = 0;
            for (int i = 0; i < fileLines.size() && drawn < maxFiles; ++i) {
                QString fn = QFileInfo(fileLines[i]).fileName();
                if (fn.isEmpty()) fn = fileLines[i];
                painter->drawText(leftX, contentY + 14 + drawn * 18, "📄 " + fn);
                drawn++;
            }
        }
    } else {
        // Check if text is a Color code (#HEX)
        static const QRegularExpression hexRegex("^#([0-9A-Fa-f]{6}|[0-9A-Fa-f]{8}|[0-9A-Fa-f]{3})$");
        QString trimmedText = summary.trimmed();
        if (hexRegex.match(trimmedText).hasMatch()) {
            QColor swatch(trimmedText);
            if (swatch.isValid()) {
                // Draw color circle swatch
                painter->setPen(QPen(ThemeManager::instance()->glassBorderColor(), 1));
                painter->setBrush(swatch);
                painter->drawEllipse(leftX, contentY + 3, 14, 14);
                painter->setBrush(Qt::NoBrush);
                painter->setPen(ThemeManager::instance()->textColor());
                painter->drawText(leftX + 22, contentY + 14, trimmedText);
                painter->restore();
                return;
            }
        }

        // Text / Note with dynamic multi-line support (strictly clipped to actual lines capped at maxLines)
        int maxAllowedLines = qBound(1, cfg->display.textMaxLines, 5);
        int lineH = getTextFm().lineSpacing();

        QString displayText = note.isEmpty() ? summary : QString("📝 %1\n%2").arg(note, summary);
        int availW = rect.width() - 28;
        QRect bound = getTextFm().boundingRect(QRect(0, 0, availW, 2000), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, displayText);
        int actualLines = qMax(1, (bound.height() + lineH / 2) / lineH);
        int displayLines = qMin(actualLines, maxAllowedLines);

        int textMaxH = displayLines * lineH;
        QRect textRect(leftX, contentY, availW, textMaxH);

        painter->save();
        painter->setClipRect(textRect);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, displayText);
        painter->restore();
    }

    painter->restore();
}
