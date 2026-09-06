#include "FluentRangeSettingCard.h"

FluentRangeSettingCard::FluentRangeSettingCard(const QString& title, const QString& content, QWidget* parent)
    : FluentSettingCard(title, content, parent) {
    setupSpinBox();
}

FluentRangeSettingCard::FluentRangeSettingCard(FluentIconType icon, const QString& title, const QString& content, QWidget* parent)
    : FluentSettingCard(icon, title, content, parent) {
    setupSpinBox();
}

void FluentRangeSettingCard::setupSpinBox() {
    m_spinBox = new QSpinBox(this);
    m_spinBox->setFixedWidth(108);
    m_spinBox->setFixedHeight(32);
    m_spinBox->setAlignment(Qt::AlignCenter);
    setTrailingWidget(m_spinBox);
    connect(m_spinBox, &QSpinBox::valueChanged, this, &FluentRangeSettingCard::valueChanged);
}

void FluentRangeSettingCard::setRange(int min, int max) {
    if (m_spinBox) {
        m_spinBox->setRange(min, max);
    }
}

void FluentRangeSettingCard::setSuffix(const QString& suffix) {
    if (m_spinBox) {
        m_spinBox->setSuffix(suffix);
    }
}

void FluentRangeSettingCard::setValue(int val) {
    if (m_spinBox) {
        m_spinBox->setValue(val);
    }
}

int FluentRangeSettingCard::value() const {
    return m_spinBox ? m_spinBox->value() : 0;
}
