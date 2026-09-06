#include "AutoStartManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <windows.h>

static const QString REG_RUN_KEY = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const QString APP_NAME = "SnapParse";

bool AutoStartManager::isAutoStartEnabled() {
    QSettings settings(REG_RUN_KEY, QSettings::NativeFormat);
    return settings.contains(APP_NAME);
}

bool AutoStartManager::setAutoStartEnabled(bool enabled) {
    QSettings settings(REG_RUN_KEY, QSettings::NativeFormat);
    if (enabled) {
        QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        settings.setValue(APP_NAME, QString("\"%1\" --autostart").arg(appPath));
    } else {
        settings.remove(APP_NAME);
    }
    settings.sync();
    return settings.status() == QSettings::NoError;
}
