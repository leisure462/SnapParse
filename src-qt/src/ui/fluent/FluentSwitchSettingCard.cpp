#include "FluentSwitchSettingCard.h"

FluentSwitchSettingCard::FluentSwitchSettingCard(const QString& title, const QString& content, QWidget* parent)
    : FluentSettingCard(title, content, parent) {
    setupSwitch();
}

FluentSwitchSettingCard::FluentSwitchSettingCard(FluentIconType icon, const QString& title, const QString& content, QWidget* parent)
    : FluentSettingCard(icon, title, content, parent) {
    setupSwitch();
}

void FluentSwitchSettingCard::setupSwitch() {
    m_switch = new SwitchWidget(this);
    setTrailingWidget(m_switch);
    connect(m_switch, &SwitchWidget::toggled, this, &FluentSwitchSettingCard::checkedChanged);
}

bool FluentSwitchSettingCard::isChecked() const {
    return m_switch ? m_switch->isChecked() : false;
}

void FluentSwitchSettingCard::setChecked(bool checked) {
    if (m_switch) {
        m_switch->setChecked(checked);
    }
}
