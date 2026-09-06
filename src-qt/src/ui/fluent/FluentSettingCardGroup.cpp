#include "FluentSettingCardGroup.h"
#include "ThemeManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QFrame>

FluentSettingCardGroup::FluentSettingCardGroup(const QString& title, QWidget* parent)
    : QWidget(parent) {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 8, 0, 8);
    m_mainLayout->setSpacing(8);

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setStyleSheet("font-size: 15px; font-weight: 600;");
    m_titleLabel->setVisible(!title.isEmpty());
    m_mainLayout->addWidget(m_titleLabel);

    m_cardContainer = new QWidget(this);
    m_cardsLayout = new QVBoxLayout(m_cardContainer);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(0);
    m_mainLayout->addWidget(m_cardContainer);

    auto updateTheme = [this]() {
        bool dark = ThemeManager::instance()->isDarkMode();
        if (m_titleLabel) {
            m_titleLabel->setStyleSheet(QString("font-size: 14px; font-weight: 600; color: %1;")
                .arg(dark ? "#ffffff" : "#1d1d1f"));
        }
        if (m_cardContainer) {
            QString bg = dark ? "rgba(38, 38, 44, 0.85)" : "rgba(255, 255, 255, 0.9)";
            QString border = dark ? "rgba(255, 255, 255, 0.1)" : "rgba(0, 0, 0, 0.08)";
            m_cardContainer->setStyleSheet(QString(R"(
                QWidget#CardContainer {
                    background-color: %1;
                    border: 1px solid %2;
                    border-radius: 10px;
                }
            )").arg(bg, border));
        }
    };

    m_cardContainer->setObjectName("CardContainer");
    connect(ThemeManager::instance(), &ThemeManager::themeApplied, this, updateTheme);
    updateTheme();
}

void FluentSettingCardGroup::setTitle(const QString& title) {
    if (m_titleLabel) {
        m_titleLabel->setText(title);
        m_titleLabel->setVisible(!title.isEmpty());
    }
}

void FluentSettingCardGroup::addSettingCard(FluentSettingCard* card) {
    if (!card) return;

    if (!m_cards.isEmpty()) {
        // Add 1px subtle divider line between cards
        QFrame* line = new QFrame(m_cardContainer);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Plain);
        line->setFixedHeight(1);
        auto updateLine = [line]() {
            bool dark = ThemeManager::instance()->isDarkMode();
            line->setStyleSheet(QString("background-color: %1; border: none;").arg(
                dark ? "rgba(255, 255, 255, 0.08)" : "rgba(0, 0, 0, 0.06)"
            ));
        };
        connect(ThemeManager::instance(), &ThemeManager::themeApplied, line, updateLine);
        updateLine();
        m_cardsLayout->addWidget(line);
    }

    m_cards.append(card);
    m_cardsLayout->addWidget(card);
}

void FluentSettingCardGroup::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
}
