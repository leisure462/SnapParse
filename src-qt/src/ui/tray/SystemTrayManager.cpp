#include "SystemTrayManager.h"
#include "EventBus.h"
#include "ClipboardMonitor.h"
#include "AppConfig.h"
#include "Logger.h"
#include "FluentIcon.h"
#include "ThemeManager.h"
#include <QApplication>
#include <windows.h>

SystemTrayManager* SystemTrayManager::instance() {
    static SystemTrayManager s_instance;
    return &s_instance;
}

SystemTrayManager::SystemTrayManager(QObject* parent) : QObject(parent) {
}

SystemTrayManager::~SystemTrayManager() {
    delete m_trayMenu;
    m_trayMenu = nullptr;
}

void SystemTrayManager::init() {
    Logger::info("SystemTrayManager::init() starting...");

    if (!m_trayIcon) {
        m_trayIcon = new QSystemTrayIcon(this);
        m_trayIcon->setToolTip("SnapParse 剪贴板管理");
        int trayIconSize = GetSystemMetrics(SM_CXSMICON);
        if (trayIconSize <= 0) trayIconSize = 16;
        m_trayIcon->setIcon(FluentIcon::trayIcon(trayIconSize));
        Logger::info(QString("SystemTrayManager: icon set at %1px").arg(trayIconSize));

        // Menu - parent to this so it's auto-deleted with SystemTrayManager
        m_trayMenu = new QMenu();
        m_openClipAction = m_trayMenu->addAction("打开剪贴板");
        m_openPrefAction = m_trayMenu->addAction("偏好设置...");
        m_trayMenu->addSeparator();

        m_listenAction = m_trayMenu->addAction("暂停监听");
        m_trayMenu->addSeparator();

        m_quitAction = m_trayMenu->addAction("退出 SnapParse");

        connect(m_openClipAction, &QAction::triggered, this, []() {
            EventBus::instance()->toggleClipboardWindow();
        });
        connect(m_openPrefAction, &QAction::triggered, this, []() {
            EventBus::instance()->showPreferencesWindow();
        });
        connect(m_listenAction, &QAction::triggered, this, &SystemTrayManager::handleToggleListening);
        connect(m_quitAction, &QAction::triggered, qApp, &QApplication::quit);

        updateMenuIcons();
        connect(ThemeManager::instance(), &ThemeManager::themeApplied, this, &SystemTrayManager::updateMenuIcons);

        m_trayIcon->setContextMenu(m_trayMenu);
        connect(m_trayIcon, &QSystemTrayIcon::activated, this, &SystemTrayManager::handleActivated);
    }

    auto cfg = AppConfig::instance();
    setVisible(cfg->general.trayIcon);

    Logger::info(QString("SystemTrayManager: tray icon shown, isVisible=%1")
        .arg(m_trayIcon->isVisible() ? "true" : "false"));

    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        Logger::info("SystemTrayManager: system tray is available");
    } else {
        Logger::warn("SystemTrayManager: system tray is NOT available");
    }
}

void SystemTrayManager::updateMenuIcons() {
    if (!m_trayMenu) return;

    bool dark = ThemeManager::instance()->isDarkMode();
    QColor iconColor = dark ? QColor("#e0e0e5") : QColor("#444448");

    if (m_openClipAction) m_openClipAction->setIcon(FluentIcon::make(FluentIconType::Reuse, iconColor, 14));
    if (m_openPrefAction) m_openPrefAction->setIcon(FluentIcon::make(FluentIconType::Settings, iconColor, 14));
    if (m_listenAction) m_listenAction->setIcon(FluentIcon::make(FluentIconType::Record, iconColor, 14));
    if (m_quitAction) m_quitAction->setIcon(FluentIcon::make(FluentIconType::Close, iconColor, 14));
}

void SystemTrayManager::setVisible(bool visible) {
    if (m_trayIcon) {
        if (visible) {
            m_trayIcon->show();
        } else {
            m_trayIcon->hide();
        }
    }
}

void SystemTrayManager::handleActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        EventBus::instance()->toggleClipboardWindow();
    }
}

void SystemTrayManager::handleToggleListening() {
    m_isListening = !m_isListening;
    if (m_isListening) {
        ClipboardMonitor::instance()->start();
        m_listenAction->setText("暂停监听");
    } else {
        ClipboardMonitor::instance()->stop();
        m_listenAction->setText("恢复监听");
    }
}
