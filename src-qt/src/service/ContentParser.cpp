#include "ContentParser.h"
#include "AppPaths.h"
#include <QRegularExpression>
#include <QFileInfo>
#include <QBuffer>
#include <QUuid>

QString ContentParser::detectSubKind(const QString& text) {
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return "";

    // URL
    static const QRegularExpression urlRegex(R"(^(https?|ftp|file):\/\/[^\s/$.?#].[^\s]*$)", QRegularExpression::CaseInsensitiveOption);
    if (urlRegex.match(trimmed).hasMatch()) return "url";

    // Email
    static const QRegularExpression emailRegex(R"(^[a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\.[a-zA-Z0-9-.]+$)");
    if (emailRegex.match(trimmed).hasMatch()) return "email";

    // Color (HEX, RGB, RGBA, HSL)
    static const QRegularExpression hexColorRegex(R"(^#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3}|[A-Fa-f0-9]{8})$)");
    static const QRegularExpression rgbColorRegex(R"(^rgba?\(\s*\d+\s*,\s*\d+\s*,\s*\d+\s*(?:,\s*[\d.]+\s*)?\)$)", QRegularExpression::CaseInsensitiveOption);
    if (hexColorRegex.match(trimmed).hasMatch() || rgbColorRegex.match(trimmed).hasMatch()) return "color";

    // Local Path
    if (QFileInfo::exists(trimmed)) return "path";

    return "";
}

QString ContentParser::computeHash(const QByteArray& data) {
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

QString ContentParser::computeHash(const QString& text) {
    return computeHash(text.toUtf8());
}

QString ContentParser::generateSummary(const QString& text, int maxLines) {
    QStringList lines = text.split('\n');
    QStringList result;
    int count = 0;
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            result.append(trimmed);
            count++;
            if (count >= maxLines) break;
        }
    }
    return result.join(" ");
}

QString ContentParser::saveImageAndGetFilename(const QImage& image, int& outWidth, int& outHeight, qint64& outSize) {
    outWidth = image.width();
    outHeight = image.height();

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    outSize = bytes.size();

    QString hash = computeHash(bytes);
    QString filename = hash + ".png";
    QString fullPath = AppPaths::imageCacheDir() + "/" + filename;

    if (!QFile::exists(fullPath)) {
        image.save(fullPath, "PNG");
    }

    return filename;
}
