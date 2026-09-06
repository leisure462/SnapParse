#include "StorageMeterWidget.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

StorageMeterWidget::StorageMeterWidget(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(4);

    setStyleSheet("background: rgba(128, 128, 128, 0.08); border-radius: 8px;");

    QHBoxLayout* topLayout = new QHBoxLayout();
    m_titleLabel = new QLabel("本地存储", this);
    m_titleLabel->setStyleSheet("font-weight: 600; font-size: 12px;");

    m_usageLabel = new QLabel("-- / --", this);
    auto updateColors = [this]() {
        QString sec = ThemeManager::instance()->secondaryTextColor().name();
        m_usageLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(sec));
    };
    updateColors();
    connect(ThemeManager::instance(), &ThemeManager::themeApplied, this, updateColors);

    topLayout->addWidget(m_titleLabel);
    topLayout->addStretch();
    topLayout->addWidget(m_usageLabel);
    layout->addLayout(topLayout);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(4);
    m_progressBar->setTextVisible(false);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setStyleSheet(R"(
        QProgressBar {
            background: rgba(128, 128, 128, 0.2);
            border-radius: 2px;
            border: none;
        }
        QProgressBar::chunk {
            background: #52c41a;
            border-radius: 2px;
        }
    )");
    layout->addWidget(m_progressBar);
}

QString StorageMeterWidget::formatBytes(qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    double gb = bytes / (1024.0 * 1024.0 * 1024.0);
    if (std::abs(gb - std::round(gb)) < 0.05) {
        return QString("%1 GB").arg(static_cast<int>(std::round(gb)));
    }
    return QString("%1 GB").arg(gb, 0, 'f', 1);
}

void StorageMeterWidget::setUsage(qint64 usedBytes, qint64 maxBytes) {
    if (maxBytes <= 0) maxBytes = 1024ULL * 1024 * 1024;
    int percent = qBound(0, static_cast<int>((usedBytes * 100) / maxBytes), 100);

    m_usageLabel->setText(QString("%1 / %2").arg(formatBytes(usedBytes), formatBytes(maxBytes)));
    m_progressBar->setValue(percent);

    if (percent > 85) {
        m_progressBar->setStyleSheet("QProgressBar { background: rgba(128,128,128,0.2); border:none; border-radius:2px;} QProgressBar::chunk { background: #ff4d4f; border-radius:2px;}");
    } else if (percent > 60) {
        m_progressBar->setStyleSheet("QProgressBar { background: rgba(128,128,128,0.2); border:none; border-radius:2px;} QProgressBar::chunk { background: #faad14; border-radius:2px;}");
    } else {
        m_progressBar->setStyleSheet("QProgressBar { background: rgba(128,128,128,0.2); border:none; border-radius:2px;} QProgressBar::chunk { background: #52c41a; border-radius:2px;}");
    }
}
