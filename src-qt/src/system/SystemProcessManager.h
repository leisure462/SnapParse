#pragma once

#include <QString>
#include <QList>
#include "Models.h"

class SystemProcessManager {
public:
    // Enumerates currently running applications from system (Task Manager / Processes / Windows)
    static QList<ClipboardApp> listRunningApps();

    // Retrieves human-friendly application name from executable FileVersionInfo (e.g. "Google Chrome")
    static QString getAppFriendlyName(const QString& exePath, const QString& defaultName);

    // Extracts high-resolution application icon and caches it as PNG
    static QString extractAndCacheAppIcon(const QString& exePath, const QString& appId);
};
