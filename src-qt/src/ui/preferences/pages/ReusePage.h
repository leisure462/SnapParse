#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include "FluentSettingCardGroup.h"
#include "FluentSwitchSettingCard.h"
#include "FluentOptionsSettingCard.h"
#include "FluentPushSettingCard.h"

class ReusePage : public QScrollArea {
    Q_OBJECT
public:
    explicit ReusePage(QWidget* parent = nullptr);

    void scrollToSetting(const QString& settingId);
    void showSection(int index);

private:
    void setupUi();

    QVBoxLayout* m_contentLayout = nullptr;
    QMap<QString, FluentSettingCard*> m_rows;

    FluentSettingCardGroup* m_pasteGroup = nullptr;
    FluentSettingCardGroup* m_copyGroup = nullptr;
    FluentSettingCardGroup* m_actionsGroup = nullptr;
};
