#include "AppPaths.h"
#include <QCoreApplication>
#include <QSettings>

QString AppPaths::s_currentDataDir = "";

void AppPaths::init() {
    QSettings settings(configFilePath(), QSettings::IniFormat);
    s_currentDataDir = settings.value("storage/customDataDir", "").toString();
    
    // Ensure standard directories exist
    QDir().mkpath(currentDataDir());
    QDir().mkpath(logDir());
    QDir().mkpath(imageCacheDir());
    QDir().mkpath(iconCacheDir());
}

QString AppPaths::appDataDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
}

QString AppPaths::defaultDataDir() {
    QString path = appDataDir() + "/data";
    QDir().mkpath(path);
    return path;
}

QString AppPaths::currentDataDir() {
    if (!s_currentDataDir.isEmpty() && QDir(s_currentDataDir).exists()) {
        return s_currentDataDir;
    }
    return defaultDataDir();
}

void AppPaths::setCurrentDataDir(const QString& path) {
    s_currentDataDir = path;
    QSettings settings(configFilePath(), QSettings::IniFormat);
    settings.setValue("storage/customDataDir", path);
    QDir().mkpath(currentDataDir());
    QDir().mkpath(imageCacheDir());
    QDir().mkpath(iconCacheDir());
}

bool AppPaths::isCustomDataDir() {
    return !s_currentDataDir.isEmpty() && s_currentDataDir != defaultDataDir();
}

QString AppPaths::databasePath() {
    return databasePathFor(currentDataDir());
}

QString AppPaths::databasePathFor(const QString& dataDir) {
    return dataDir + "/ecopaste.db";
}

QString AppPaths::configFilePath() {
    QString path = appDataDir();
    QDir().mkpath(path);
    return path + "/settings.ini";
}

QString AppPaths::logDir() {
    QString path = appDataDir() + "/logs";
    QDir().mkpath(path);
    return path;
}

QString AppPaths::imageCacheDir() {
    return imageCacheDirFor(currentDataDir());
}

QString AppPaths::imageCacheDirFor(const QString& dataDir) {
    QString path = dataDir + "/images";
    QDir().mkpath(path);
    return path;
}

QString AppPaths::iconCacheDir() {
    return iconCacheDirFor(currentDataDir());
}

QString AppPaths::iconCacheDirFor(const QString& dataDir) {
    QString path = dataDir + "/icons";
    QDir().mkpath(path);
    return path;
}
