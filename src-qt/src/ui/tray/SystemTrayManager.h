#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>

class SystemTrayManager : public QObject {
    Q_OBJECT
public:
    static SystemTrayManager* instance();

    void init();
    void setVisible(bool visible);

private slots:
    void handleActivated(QSystemTrayIcon::ActivationReason reason);
    void handleToggleListening();
    void updateMenuIcons();

private:
    explicit SystemTrayManager(QObject* parent = nullptr);
    ~SystemTrayManager();

    QSystemTrayIcon* m_trayIcon = nullptr;
    QMenu* m_trayMenu = nullptr;
    QAction* m_openClipAction = nullptr;
    QAction* m_openPrefAction = nullptr;
    QAction* m_listenAction = nullptr;
    QAction* m_quitAction = nullptr;
    bool m_isListening = true;
};
