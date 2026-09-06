#include "MemoryManager.h"
#include "ClipboardCardDelegate.h"
#include "Logger.h"
#include <QPixmapCache>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

MemoryManager* MemoryManager::instance() {
    static MemoryManager s_instance;
    return &s_instance;
}

MemoryManager::MemoryManager(QObject* parent) : QObject(parent) {
    m_trimTimer = new QTimer(this);
    m_trimTimer->setSingleShot(true);
    m_trimTimer->setInterval(3000); // 3 seconds after entering idle background
    connect(m_trimTimer, &QTimer::timeout, this, &MemoryManager::handleIdleTrim);

    QTimer* periodicTimer = new QTimer(this);
    periodicTimer->setInterval(60000); // Every 60 seconds maintain lean memory
    connect(periodicTimer, &QTimer::timeout, this, [this]() {
        if (m_visibleWindowCount == 0) {
            trimWorkingSetImmediately();
        }
    });
    periodicTimer->start();
}

void MemoryManager::init() {
    // Set a reasonable limit on Qt's internal pixmap cache (16MB instead of default large footprint)
    QPixmapCache::setCacheLimit(16 * 1024);
    Logger::info("MemoryManager initialized with 16MB PixmapCache limit");
}

void MemoryManager::notifyWindowShown() {
    m_visibleWindowCount++;
    if (m_trimTimer->isActive()) {
        m_trimTimer->stop();
    }
}

void MemoryManager::notifyWindowHidden() {
    if (m_visibleWindowCount > 0) {
        m_visibleWindowCount--;
    }
    if (m_visibleWindowCount <= 0) {
        m_visibleWindowCount = 0;
        // Schedule background memory trimming
        m_trimTimer->start();
    }
}

void MemoryManager::trimWorkingSetImmediately() {
    QPixmapCache::clear();
    ClipboardCardDelegate::clearThumbnailCache();

#ifdef Q_OS_WIN
    HANDLE hProcess = GetCurrentProcess();
    SetProcessWorkingSetSize(hProcess, static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
    EmptyWorkingSet(hProcess);
#endif
    Logger::info("MemoryManager: trimmed process working set and freed background caches");
}

void MemoryManager::handleIdleTrim() {
    if (m_visibleWindowCount == 0) {
        trimWorkingSetImmediately();
    }
}
