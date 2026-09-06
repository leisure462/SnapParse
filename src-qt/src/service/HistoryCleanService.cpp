#include "HistoryCleanService.h"
#include "AppConfig.h"
#include "DatabaseManager.h"
#include "ClipboardCardDelegate.h"
#include "EventBus.h"
#include "Logger.h"

HistoryCleanService* HistoryCleanService::instance() {
    static HistoryCleanService s_instance;
    return &s_instance;
}

HistoryCleanService::HistoryCleanService(QObject* parent) : QObject(parent) {
    connect(&m_timer, &QTimer::timeout, this, &HistoryCleanService::runCleanup);
}

void HistoryCleanService::start() {
    // Run once on startup
    runCleanup();

    int hours = AppConfig::instance()->history.cleanupIntervalHours;
    if (hours > 0) {
        m_timer.start(hours * 3600 * 1000);
    } else {
        m_timer.stop();
    }
}

void HistoryCleanService::stop() {
    m_timer.stop();
}

void HistoryCleanService::runCleanup() {
    auto config = AppConfig::instance();
    int retention = config->history.retentionDays;
    int maxCount = config->history.maxCount;

    if (retention > 0 || maxCount > 0) {
        int deleted = DatabaseManager::instance()->cleanupOldHistory(retention, maxCount);
        if (deleted > 0) {
            Logger::info(QString("Cleaned up %1 old history records").arg(deleted));
            DatabaseManager::instance()->cleanupUnreferencedCache();
            ClipboardCardDelegate::clearThumbnailCache();
            EventBus::instance()->clipboardUpdated();
        }
    }
}
