#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include "FluentSettingCardGroup.h"
#include "FluentSwitchSettingCard.h"
#include "FluentPushSettingCard.h"

class DataPage : public QScrollArea {
    Q_OBJECT
public:
    explicit DataPage(QWidget* parent = nullptr);

    void scrollToSetting(const QString& settingId);
    void showSection(int index);

private:
    void setupUi();

    QVBoxLayout* m_contentLayout = nullptr;
    QMap<QString, FluentSettingCard*> m_rows;

    FluentSettingCardGroup* m_storageGroup = nullptr;
    FluentSettingCardGroup* m_backupGroup = nullptr;
    FluentSettingCardGroup* m_diagGroup = nullptr;
    FluentSettingCardGroup* m_updateGroup = nullptr;
};
