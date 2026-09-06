#pragma once

#include "FluentSettingCard.h"
#include <QComboBox>
#include <QFontDatabase>

class FluentFontSettingCard : public FluentSettingCard {
    Q_OBJECT
public:
    explicit FluentFontSettingCard(const QString& title, const QString& content = "", QWidget* parent = nullptr);
    explicit FluentFontSettingCard(FluentIconType icon, const QString& title, const QString& content = "", QWidget* parent = nullptr);

    void setCurrentFont(const QString& family);
    QString currentFont() const;

signals:
    void fontChanged(const QString& family);

private:
    void setupComboBox();

    QComboBox* m_comboBox = nullptr;
};
