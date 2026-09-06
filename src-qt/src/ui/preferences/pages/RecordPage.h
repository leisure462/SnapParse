#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include "FluentSettingCardGroup.h"
#include "FluentSwitchSettingCard.h"
#include "FluentRangeSettingCard.h"
#include "FluentPushSettingCard.h"
#include "FluentAppFilterWidget.h"

class RecordPage : public QScrollArea {
    Q_OBJECT
public:
    explicit RecordPage(QWidget* parent = nullptr);

    void scrollToSetting(const QString& settingId);
    void showSection(int index);

private:
    void setupUi();
    void loadData();

    QVBoxLayout* m_contentLayout = nullptr;
    QMap<QString, QWidget*> m_rows;
    FluentAppFilterWidget* m_appFilter = nullptr;

    FluentSettingCardGroup* m_captureGroup = nullptr;
    QWidget* m_sourceGroup = nullptr;
    FluentSettingCardGroup* m_sensitiveGroup = nullptr;
};
