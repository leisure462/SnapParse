#pragma once

#include <QString>
#include <QStringList>
#include <QImage>
#include <QCryptographicHash>
#include "Models.h"

class ContentParser {
public:
    static QString detectSubKind(const QString& text);
    static QString computeHash(const QByteArray& data);
    static QString computeHash(const QString& text);
    static QString generateSummary(const QString& text, int maxLines = 3);
    static QString saveImageAndGetFilename(const QImage& image, int& outWidth, int& outHeight, qint64& outSize);
};
