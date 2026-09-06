#pragma once

#include <QWidget>
#include <QLabel>
#include <QProgressBar>

class StorageMeterWidget : public QWidget {
    Q_OBJECT
public:
    explicit StorageMeterWidget(QWidget* parent = nullptr);

    void setUsage(qint64 usedBytes, qint64 maxBytes = 1024ULL * 1024 * 1024);

private:
    QString formatBytes(qint64 bytes);

    QLabel* m_titleLabel = nullptr;
    QLabel* m_usageLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
};
