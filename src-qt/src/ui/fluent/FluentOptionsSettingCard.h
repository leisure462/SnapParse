#pragma once

#include "FluentSettingCard.h"
#include "SegmentedWidget.h"
#include <QComboBox>

class FluentOptionsSettingCard : public FluentSettingCard {
    Q_OBJECT
public:
    explicit FluentOptionsSettingCard(const QString& title, const QString& content = "", QWidget* parent = nullptr);
    explicit FluentOptionsSettingCard(FluentIconType icon, const QString& title, const QString& content = "", QWidget* parent = nullptr);

    void setOptions(const QList<QPair<QString, QString>>& options);
    void setCurrentValue(const QString& value);
    QString currentValue() const;

signals:
    void valueChanged(const QString& value);

private:
    void setupSegmented();
    SegmentedWidget* m_segmented = nullptr;
};
