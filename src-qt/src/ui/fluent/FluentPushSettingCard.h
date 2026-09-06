#pragma once

#include "FluentSettingCard.h"
#include <QPushButton>

class FluentPushSettingCard : public FluentSettingCard {
    Q_OBJECT
public:
    explicit FluentPushSettingCard(const QString& buttonText, const QString& title, const QString& content = "", QWidget* parent = nullptr);
    explicit FluentPushSettingCard(FluentIconType icon, const QString& buttonText, const QString& title, const QString& content = "", QWidget* parent = nullptr);

    void setButtonText(const QString& text);
    QPushButton* button() const { return m_button; }

signals:
    void clicked();

private:
    void setupButton(const QString& text);
    QPushButton* m_button = nullptr;
};
