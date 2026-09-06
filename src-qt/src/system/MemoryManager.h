#pragma once

#include <QObject>
#include <QTimer>

class MemoryManager : public QObject {
    Q_OBJECT
public:
    static MemoryManager* instance();

    void init();
    void notifyWindowShown();
    void notifyWindowHidden();
    void trimWorkingSetImmediately();

private slots:
    void handleIdleTrim();

private:
    explicit MemoryManager(QObject* parent = nullptr);

    QTimer* m_trimTimer = nullptr;
    int m_visibleWindowCount = 0;
};
