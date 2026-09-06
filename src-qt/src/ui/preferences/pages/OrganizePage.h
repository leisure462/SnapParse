#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include "FluentSettingCardGroup.h"
#include "FluentSwitchSettingCard.h"
#include "FluentOptionsSettingCard.h"
#include "FluentRangeSettingCard.h"
#include "FluentPushSettingCard.h"

class OrganizePage : public QScrollArea {
    Q_OBJECT
public:
    explicit OrganizePage(QWidget* parent = nullptr);

    void scrollToSetting(const QString& settingId);
    void showSection(int index);

private:
    void setupUi();

    QVBoxLayout* m_contentLayout = nullptr;
    QMap<QString, FluentSettingCard*> m_rows;

    FluentSettingCardGroup* m_retentionGroup = nullptr;
    FluentSettingCardGroup* m_favGroup = nullptr;
    FluentSettingCardGroup* m_searchGroup = nullptr;
};
