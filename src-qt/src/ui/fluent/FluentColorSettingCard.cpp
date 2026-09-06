#include "FluentColorSettingCard.h"
#include "ThemeManager.h"
#include <QColorDialog>
#include <QPainter>

FluentColorSettingCard::FluentColorSettingCard(FluentIconType icon, const QString& title, const QString& content, QWidget* parent)
    : FluentSettingCard(icon, title, content, parent) {

    m_presets = {
        {"极光蓝", "#1677ff"},
        {"翡翠绿", "#10b981"},
        {"鸢尾紫", "#7c3aed"},
        {"晚霞橙", "#f97316"},
        {"樱花粉", "#ec4899"},
        {"赛博青", "#06b6d4"},
        {"烈焰红", "#ef4444"},
        {"钛金灰", "#64748b"}
    };

    m_currentColorHex = "#1677ff";

    m_container = new QWidget(this);
    m_swatchesLayout = new QHBoxLayout(m_container);
    m_swatchesLayout->setContentsMargins(0, 0, 0, 0);
    m_swatchesLayout->setSpacing(8);

    setupSwatches();
    setTrailingWidget(m_container);

    connect(ThemeManager::instance(), &ThemeManager::themeApplied, this, &FluentColorSettingCard::updateSwatchesState);
}

void FluentColorSettingCard::setupSwatches() {
    for (const auto& item : m_presets) {
        QPushButton* btn = new QPushButton(m_container);
        btn->setFixedSize(24, 24);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(item.name + " (" + item.hex + ")");

        QString hex = item.hex;
        connect(btn, &QPushButton::clicked, this, [this, hex]() {
            setCurrentColor(hex);
            emit colorChanged(hex);
        });

        m_swatchesLayout->addWidget(btn);
        m_swatchButtons.append(btn);
    }

    // Custom Color Button
    m_customColorBtn = new QPushButton("🎨", m_container);
    m_customColorBtn->setFixedSize(24, 24);
    m_customColorBtn->setCursor(Qt::PointingHandCursor);
    m_customColorBtn->setToolTip("自定义颜色...");
    connect(m_customColorBtn, &QPushButton::clicked, this, &FluentColorSettingCard::handleCustomColorPick);
    m_swatchesLayout->addWidget(m_customColorBtn);

    updateSwatchesState();
}

void FluentColorSettingCard::setCurrentColor(const QString& hex) {
    if (!hex.isEmpty() && QColor::isValidColorName(hex)) {
        m_currentColorHex = hex.toLower();
        updateSwatchesState();
    }
}

void FluentColorSettingCard::updateSwatchesState() {
    bool dark = ThemeManager::instance()->isDarkMode();
    // Theme-adaptive border and highlight colors
    QString selectedBorder = dark ? "2px solid #ffffff" : "2px solid #333333";
    QString hoverBorder = dark ? "2px solid rgba(255, 255, 255, 0.8)" : "2px solid rgba(0, 0, 0, 0.5)";
    QString normalBorder = dark ? "1px solid rgba(255, 255, 255, 0.15)" : "1px solid rgba(0, 0, 0, 0.18)";
    QString checkColor = dark ? "#ffffff" : "#ffffff"; // white checkmark is fine on colored backgrounds

    bool isPresetMatch = false;
    for (int i = 0; i < m_presets.size(); ++i) {
        QPushButton* btn = m_swatchButtons[i];
        QString hex = m_presets[i].hex.toLower();
        bool isSelected = (m_currentColorHex == hex);
        if (isSelected) isPresetMatch = true;

        btn->setText(isSelected ? "✔" : "");
        QString border = isSelected ? selectedBorder : normalBorder;
        btn->setStyleSheet(QString(R"(
            QPushButton {
                background-color: %1;
                border: %2;
                border-radius: 12px;
                color: %3;
                font-size: 11px;
                font-weight: bold;
                padding: 0;
            }
            QPushButton:hover {
                border: %4;
            }
        )").arg(m_presets[i].hex, border, checkColor, hoverBorder));
    }

    // Custom color button styling
    QString customDashedBorder = dark ? "1px dashed rgba(255, 255, 255, 0.4)" : "1px dashed rgba(0, 0, 0, 0.3)";
    QString customBg = dark ? "rgba(255, 255, 255, 0.1)" : "rgba(0, 0, 0, 0.06)";
    QString customHoverBg = dark ? "rgba(255, 255, 255, 0.25)" : "rgba(0, 0, 0, 0.12)";

    if (!isPresetMatch) {
        m_customColorBtn->setText("✔");
        m_customColorBtn->setStyleSheet(QString(R"(
            QPushButton {
                background-color: %1;
                border: %2;
                border-radius: 12px;
                color: %3;
                font-size: 11px;
                font-weight: bold;
                padding: 0;
            }
            QPushButton:hover {
                border: %4;
            }
        )").arg(m_currentColorHex, selectedBorder, checkColor, hoverBorder));
    } else {
        m_customColorBtn->setText("🎨");
        m_customColorBtn->setStyleSheet(QString(R"(
            QPushButton {
                border: %1;
                border-radius: 12px;
                background: %2;
                font-size: 11px;
                padding: 0;
            }
            QPushButton:hover {
                background: %3;
            }
        )").arg(customDashedBorder, customBg, customHoverBg));
    }
}

void FluentColorSettingCard::handleCustomColorPick() {
    QColor initial(m_currentColorHex);
    QColor chosen = QColorDialog::getColor(initial, this, "选择自定义主题强调色", QColorDialog::ShowAlphaChannel);
    if (chosen.isValid()) {
        QString hex = chosen.name();
        setCurrentColor(hex);
        emit colorChanged(hex);
    }
}
