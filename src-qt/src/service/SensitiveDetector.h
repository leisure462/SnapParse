#pragma once

#include <QString>
#include <QRegularExpression>

class SensitiveDetector {
public:
    static bool isSensitive(const QString& text);
    static QString redact(const QString& text);
};
