#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include "FluentSettingCardGroup.h"
#include "FluentPushSettingCard.h"

class AboutPage : public QScrollArea {
    Q_OBJECT
public:
    explicit AboutPage(QWidget* parent = nullptr);

    void scrollToSetting(const QString& settingId);
    void showSection(int index);

private:
    void setupUi();

    QVBoxLayout* m_contentLayout = nullptr;
    QMap<QString, FluentSettingCard*> m_rows;
    FluentSettingCardGroup* m_abtGroup = nullptr;
};
