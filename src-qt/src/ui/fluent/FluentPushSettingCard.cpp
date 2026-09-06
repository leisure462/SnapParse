#include "FluentPushSettingCard.h"

FluentPushSettingCard::FluentPushSettingCard(const QString& buttonText, const QString& title, const QString& content, QWidget* parent)
    : FluentSettingCard(title, content, parent) {
    setupButton(buttonText);
}

FluentPushSettingCard::FluentPushSettingCard(FluentIconType icon, const QString& buttonText, const QString& title, const QString& content, QWidget* parent)
    : FluentSettingCard(icon, title, content, parent) {
    setupButton(buttonText);
}

void FluentPushSettingCard::setupButton(const QString& text) {
    m_button = new QPushButton(text, this);
    m_button->setCursor(Qt::PointingHandCursor);
    m_button->setFixedHeight(32);
    m_button->setMinimumWidth(88);
    setTrailingWidget(m_button);
    connect(m_button, &QPushButton::clicked, this, &FluentPushSettingCard::clicked);
}

void FluentPushSettingCard::setButtonText(const QString& text) {
    if (m_button) {
        m_button->setText(text);
    }
}
