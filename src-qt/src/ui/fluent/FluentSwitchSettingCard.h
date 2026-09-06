#pragma once

#include "FluentSettingCard.h"
#include "SwitchWidget.h"

class FluentSwitchSettingCard : public FluentSettingCard {
    Q_OBJECT
public:
    explicit FluentSwitchSettingCard(const QString& title, const QString& content = "", QWidget* parent = nullptr);
    explicit FluentSwitchSettingCard(FluentIconType icon, const QString& title, const QString& content = "", QWidget* parent = nullptr);

    bool isChecked() const;
    void setChecked(bool checked);
    SwitchWidget* switchWidget() const { return m_switch; }

signals:
    void checkedChanged(bool checked);

private:
    void setupSwitch();
    SwitchWidget* m_switch = nullptr;
};
