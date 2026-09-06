#include "QuickActionBar.h"
#include "ThemeManager.h"
#include "AppConfig.h"
#include "EventBus.h"
#include "FluentIcon.h"

QuickActionBar::QuickActionBar(QWidget* parent) : QWidget(parent) {
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(2);

    m_btnCopy = new QPushButton(this);
    m_btnCopy->setToolTip("复制");
    m_btnCopy->setCursor(Qt::PointingHandCursor);
    m_btnCopy->setIconSize(QSize(14, 14));
    connect(m_btnCopy, &QPushButton::clicked, this, &QuickActionBar::copyTriggered);
    layout->addWidget(m_btnCopy);

    m_btnPaste = new QPushButton(this);
    m_btnPaste->setToolTip("粘贴");
    m_btnPaste->setCursor(Qt::PointingHandCursor);
    m_btnPaste->setIconSize(QSize(14, 14));
    connect(m_btnPaste, &QPushButton::clicked, this, &QuickActionBar::pasteTriggered);
    layout->addWidget(m_btnPaste);

    m_btnStar = new QPushButton(this);
    m_btnStar->setToolTip("收藏");
    m_btnStar->setCursor(Qt::PointingHandCursor);
    m_btnStar->setIconSize(QSize(14, 14));
    connect(m_btnStar, &QPushButton::clicked, this, &QuickActionBar::favoriteTriggered);
    layout->addWidget(m_btnStar);

    m_btnPin = new QPushButton(this);
    m_btnPin->setToolTip("置顶");
    m_btnPin->setCursor(Qt::PointingHandCursor);
    m_btnPin->setIconSize(QSize(14, 14));
    connect(m_btnPin, &QPushButton::clicked, this, &QuickActionBar::pinTriggered);
    layout->addWidget(m_btnPin);

    m_btnNote = new QPushButton(this);
    m_btnNote->setToolTip("备注");
    m_btnNote->setCursor(Qt::PointingHandCursor);
    m_btnNote->setIconSize(QSize(14, 14));
    connect(m_btnNote, &QPushButton::clicked, this, &QuickActionBar::noteTriggered);
    layout->addWidget(m_btnNote);

    m_btnDelete = new QPushButton(this);
    m_btnDelete->setToolTip("删除");
    m_btnDelete->setCursor(Qt::PointingHandCursor);
    m_btnDelete->setIconSize(QSize(14, 14));
    connect(m_btnDelete, &QPushButton::clicked, this, &QuickActionBar::deleteTriggered);
    layout->addWidget(m_btnDelete);

    connect(ThemeManager::instance(), &ThemeManager::themeApplied, this, &QuickActionBar::updateIconsAndStyles);
    connect(EventBus::instance(), &EventBus::settingsChanged, this, [this](const QString& sec) {
        if (sec == "actions" || sec == "all") {
            updateIconsAndStyles();
        }
    });
    updateIconsAndStyles();

    setFixedHeight(26);
}

void QuickActionBar::updateIconsAndStyles() {
    bool dark = ThemeManager::instance()->isDarkMode();
    QColor primaryColor = ThemeManager::instance()->primaryColor();
    QColor iconColor = dark ? QColor("#c5c5cb") : QColor("#555558");
    QString hoverRgba = QString("rgba(%1, %2, %3, %4)").arg(primaryColor.red()).arg(primaryColor.green()).arg(primaryColor.blue()).arg(dark ? "0.30" : "0.15");

    if (dark) {
        setStyleSheet(QString(R"(
            QWidget {
                background: rgba(30, 30, 36, 0.95);
                border: 1px solid rgba(255, 255, 255, 0.10);
                border-radius: 7px;
            }
            QPushButton {
                border: none;
                background: transparent;
                padding: 2px 4px;
                border-radius: 4px;
            }
            QPushButton:hover {
                background: %1;
            }
        )").arg(hoverRgba));
    } else {
        setStyleSheet(QString(R"(
            QWidget {
                background: rgba(250, 250, 252, 0.95);
                border: 1px solid rgba(0, 0, 0, 0.08);
                border-radius: 7px;
            }
            QPushButton {
                border: none;
                background: transparent;
                padding: 2px 4px;
                border-radius: 4px;
            }
            QPushButton:hover {
                background: %1;
            }
        )").arg(hoverRgba));
    }

    auto cfg = AppConfig::instance();
    m_btnCopy->setVisible(cfg->content.itemActions.contains("copy") || cfg->content.itemActions.contains("copyPlain"));
    m_btnPaste->setVisible(cfg->content.itemActions.contains("paste") || cfg->content.itemActions.contains("pastePlain"));
    m_btnStar->setVisible(cfg->content.itemActions.contains("star"));
    m_btnPin->setVisible(cfg->content.itemActions.contains("pinItem"));
    m_btnNote->setVisible(cfg->content.itemActions.contains("note"));
    m_btnDelete->setVisible(cfg->content.itemActions.contains("delete"));

    m_btnCopy->setIcon(FluentIcon::make(FluentIconType::Copy, iconColor, 14));
    m_btnPaste->setIcon(FluentIcon::make(FluentIconType::Paste, iconColor, 14));
    m_btnStar->setIcon(FluentIcon::make(FluentIconType::Star, m_isFav ? primaryColor : iconColor, 14));
    m_btnPin->setIcon(FluentIcon::make(FluentIconType::Pin, m_isPin ? primaryColor : iconColor, 14));
    m_btnNote->setIcon(FluentIcon::make(FluentIconType::Note, iconColor, 14));
    m_btnDelete->setIcon(FluentIcon::make(FluentIconType::Trash, iconColor, 14));
}

void QuickActionBar::setItemFavorite(bool fav) {
    m_isFav = fav;
    bool dark = ThemeManager::instance()->isDarkMode();
    QColor primaryColor = ThemeManager::instance()->primaryColor();
    QColor iconColor = dark ? QColor("#c5c5cb") : QColor("#555558");
    m_btnStar->setIcon(FluentIcon::make(FluentIconType::Star, fav ? primaryColor : iconColor, 14));
}

void QuickActionBar::setItemPinned(bool pin) {
    m_isPin = pin;
    bool dark = ThemeManager::instance()->isDarkMode();
    QColor primaryColor = ThemeManager::instance()->primaryColor();
    QColor iconColor = dark ? QColor("#c5c5cb") : QColor("#555558");
    m_btnPin->setIcon(FluentIcon::make(FluentIconType::Pin, pin ? primaryColor : iconColor, 14));
}
