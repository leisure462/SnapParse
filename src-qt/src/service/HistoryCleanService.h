#pragma once

#include <QObject>
#include <QTimer>

class HistoryCleanService : public QObject {
    Q_OBJECT
public:
    static HistoryCleanService* instance();

    void start();
    void stop();
    void runCleanup();

private:
    explicit HistoryCleanService(QObject* parent = nullptr);

    QTimer m_timer;
};
