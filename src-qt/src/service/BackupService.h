#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

class BackupService {
public:
    static bool exportBackup(const QString& targetFilePath, bool encrypted, const QString& password = "");
    static bool importBackup(const QString& sourceFilePath, const QString& strategy = "merge", const QString& password = "");
};
