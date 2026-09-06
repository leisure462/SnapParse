#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QList>
#include "FluentSettingCard.h"

class FluentSettingCardGroup : public QWidget {
    Q_OBJECT
public:
    explicit FluentSettingCardGroup(const QString& title = "", QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void addSettingCard(FluentSettingCard* card);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QLabel* m_titleLabel = nullptr;
    QVBoxLayout* m_mainLayout = nullptr;
    QWidget* m_cardContainer = nullptr;
    QVBoxLayout* m_cardsLayout = nullptr;
    QList<FluentSettingCard*> m_cards;
};
