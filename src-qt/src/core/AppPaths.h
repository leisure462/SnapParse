#pragma once

#include <QString>
#include <QStandardPaths>
#include <QDir>

class AppPaths {
public:
    static void init();

    static QString appDataDir();
    static QString defaultDataDir();
    static QString currentDataDir();
    static void setCurrentDataDir(const QString& path);
    static bool isCustomDataDir();

    static QString databasePath();
    static QString databasePathFor(const QString& dataDir);
    static QString configFilePath();
    static QString logDir();
    static QString imageCacheDir();
    static QString imageCacheDirFor(const QString& dataDir);
    static QString iconCacheDir();
    static QString iconCacheDirFor(const QString& dataDir);

private:
    static QString s_currentDataDir;
};
