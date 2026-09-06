#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include "FluentSettingCardGroup.h"
#include "FluentSwitchSettingCard.h"
#include "FluentOptionsSettingCard.h"
#include "FluentRangeSettingCard.h"
#include "FluentPushSettingCard.h"

class WorkflowPage : public QScrollArea {
    Q_OBJECT
public:
    explicit WorkflowPage(QWidget* parent = nullptr);

    void scrollToSetting(const QString& settingId);
    void showSection(int index);

private:
    void setupUi();

    QVBoxLayout* m_contentLayout = nullptr;
    QMap<QString, FluentSettingCard*> m_rows;

    FluentSettingCardGroup* m_winGroup = nullptr;
    FluentSettingCardGroup* m_prvGroup = nullptr;
    FluentSettingCardGroup* m_appGroup = nullptr;
    FluentSettingCardGroup* m_ctrlGroup = nullptr;
};
