#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QRect>

class EventBus : public QObject {
    Q_OBJECT
public:
    static EventBus* instance();

signals:
    // Settings events
    void settingsChanged(const QString& section);
    void themeChanged(const QString& theme);
    void languageChanged(const QString& lang);

    // Clipboard events
    void clipboardUpdated();
    void clipboardItemAdded(const QString& id);
    void clipboardGroupsUpdated();
    void clipboardAppsUpdated();

    // Window events
    void showClipboardWindow();
    void hideClipboardWindow();
    void toggleClipboardWindow();
    void showPreferencesWindow(const QString& targetTab = "", const QString& targetSettingId = "");
    void showPreviewWindow(const QString& itemId, const QRect& anchorRect = QRect(), const QRect& windowRect = QRect());
    void hidePreviewWindow();
    void previewVisibilityChanged(bool visible);

    // Storage events
    void storageLocationChanged(const QString& newPath);
    void cacheCleaned(qint64 freedBytes);

private:
    explicit EventBus(QObject* parent = nullptr) : QObject(parent) {}
};
