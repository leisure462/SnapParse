#pragma once

#include "FluentSettingCard.h"
#include <QSpinBox>

class FluentRangeSettingCard : public FluentSettingCard {
    Q_OBJECT
public:
    explicit FluentRangeSettingCard(const QString& title, const QString& content = "", QWidget* parent = nullptr);
    explicit FluentRangeSettingCard(FluentIconType icon, const QString& title, const QString& content = "", QWidget* parent = nullptr);

    void setRange(int min, int max);
    void setSuffix(const QString& suffix);
    void setValue(int val);
    int value() const;
    QSpinBox* spinBox() const { return m_spinBox; }

signals:
    void valueChanged(int val);

private:
    void setupSpinBox();
    QSpinBox* m_spinBox = nullptr;
};
