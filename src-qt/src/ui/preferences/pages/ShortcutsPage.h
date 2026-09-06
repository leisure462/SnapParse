#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include "FluentSettingCardGroup.h"
#include "FluentSwitchSettingCard.h"

class ShortcutsPage : public QScrollArea {
    Q_OBJECT
public:
    explicit ShortcutsPage(QWidget* parent = nullptr);

    void scrollToSetting(const QString& settingId);
    void showSection(int index);

private:
    void setupUi();

    QVBoxLayout* m_contentLayout = nullptr;
    QMap<QString, FluentSettingCard*> m_rows;
    FluentSettingCardGroup* m_scGroup = nullptr;
};
