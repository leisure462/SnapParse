#include "FluentOptionsSettingCard.h"

FluentOptionsSettingCard::FluentOptionsSettingCard(const QString& title, const QString& content, QWidget* parent)
    : FluentSettingCard(title, content, parent) {
    setupSegmented();
}

FluentOptionsSettingCard::FluentOptionsSettingCard(FluentIconType icon, const QString& title, const QString& content, QWidget* parent)
    : FluentSettingCard(icon, title, content, parent) {
    setupSegmented();
}

void FluentOptionsSettingCard::setupSegmented() {
    m_segmented = new SegmentedWidget(this);
    setTrailingWidget(m_segmented);
    connect(m_segmented, &SegmentedWidget::valueChanged, this, &FluentOptionsSettingCard::valueChanged);
}

void FluentOptionsSettingCard::setOptions(const QList<QPair<QString, QString>>& options) {
    if (m_segmented) {
        m_segmented->setOptions(options);
    }
}

void FluentOptionsSettingCard::setCurrentValue(const QString& value) {
    if (m_segmented) {
        m_segmented->setCurrentValue(value);
    }
}

QString FluentOptionsSettingCard::currentValue() const {
    return m_segmented ? m_segmented->currentValue() : "";
}
