#pragma once

#include "FluentSettingCard.h"
#include <QPushButton>
#include <QHBoxLayout>
#include <QList>

struct PresetColorItem {
    QString name;
    QString hex;
};

class FluentColorSettingCard : public FluentSettingCard {
    Q_OBJECT
public:
    explicit FluentColorSettingCard(FluentIconType icon, const QString& title, const QString& content = "", QWidget* parent = nullptr);

    void setCurrentColor(const QString& hex);
    QString currentColor() const { return m_currentColorHex; }

signals:
    void colorChanged(const QString& hex);

private slots:
    void handleCustomColorPick();

private:
    void setupSwatches();
    void updateSwatchesState();

    QList<PresetColorItem> m_presets;
    QString m_currentColorHex;
    QWidget* m_container = nullptr;
    QHBoxLayout* m_swatchesLayout = nullptr;
    QList<QPushButton*> m_swatchButtons;
    QPushButton* m_customColorBtn = nullptr;
};
