#include "HeaderBar.h"
#include "AppConfig.h"
#include "ThemeManager.h"
#include "EventBus.h"
#include "Logger.h"
#include "FluentIcon.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QMouseEvent>
#include <QWindow>
#include <cmath>
#include <windows.h>

HeaderBar::HeaderBar(QWidget* parent) : QWidget(parent) {
    Logger::info("HeaderBar constructor start");
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 4);
    rootLayout->setSpacing(8);

    // ==========================================
    // Row 1: Top Bar (App Icon, Search, Pin, Settings)
    // ==========================================
    QHBoxLayout* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(8);

    // 1. App Icon Badge
    m_appIcon = new QLabel(this);
    m_appIcon->setFixedSize(28, 28);
    m_appIcon->setAlignment(Qt::AlignCenter);
    m_appIcon->setToolTip("SnapParse 剪贴板管理");
    m_appIcon->setPixmap(FluentIcon::appIcon(24, true).pixmap(24, 24));
    topRow->addWidget(m_appIcon);

    Logger::info("HeaderBar: creating searchEdit...");
    m_searchEdit = new FluentSearchLineEdit(this);
    m_searchEdit->setPlaceholderText("搜索历史记录 (Ctrl+F)...");
    m_searchEdit->setFixedHeight(28);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString&) {
        emit filterChanged();
    });
    topRow->addWidget(m_searchEdit, 1);

    Logger::info("HeaderBar: creating pinBtn & settingsBtn...");
    m_pinBtn = new QPushButton(this);
    m_pinBtn->setCheckable(true);
    m_pinBtn->setFixedSize(28, 28);
    m_pinBtn->setIconSize(QSize(18, 18));
    m_pinBtn->setCursor(Qt::PointingHandCursor);
    m_pinBtn->setToolTip("窗口置顶");
    connect(m_pinBtn, &QPushButton::toggled, this, [this](bool chk) {
        bool dark = ThemeManager::instance()->isDarkMode();
        QColor topIconCol = dark ? QColor("#c5c5cb") : QColor("#555558");
        m_pinBtn->setIcon(FluentIcon::make(FluentIconType::Pin, chk ? QColor("#ffffff") : topIconCol, 15));
        emit pinToggled(chk);
    });
    topRow->addWidget(m_pinBtn);

    m_settingsBtn = new QPushButton(this);
    m_settingsBtn->setFixedSize(28, 28);
    m_settingsBtn->setIconSize(QSize(18, 18));
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    m_settingsBtn->setToolTip("偏好设置 (Alt+X)");
    connect(m_settingsBtn, &QPushButton::clicked, this, []() {
        EventBus::instance()->showPreferencesWindow();
    });
    topRow->addWidget(m_settingsBtn);

    rootLayout->addLayout(topRow);

    Logger::info("HeaderBar: creating categories...");
    QHBoxLayout* catRow = new QHBoxLayout();
    catRow->setContentsMargins(0, 0, 0, 0);
    catRow->setSpacing(6);

    m_categoryGroup = new QButtonGroup(this);
    connect(m_categoryGroup, &QButtonGroup::idClicked, this, &HeaderBar::handleCategoryClicked);

    struct CatDef { QString id; FluentIconType iconType; QString tip; };
    QList<CatDef> defs = {
        {"all",      FluentIconType::Reuse,    "全部"},
        {"favorite", FluentIconType::Star,     "收藏"},
        {"text",     FluentIconType::FileText, "文本"},
        {"image",    FluentIconType::Image,    "图片"},
        {"files",    FluentIconType::Folder,   "文件"},
        {"link",     FluentIconType::Globe,    "链接"}
    };

    for (int i = 0; i < defs.size(); ++i) {
        QPushButton* btn = new QPushButton(this);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(defs[i].tip);
        btn->setIconSize(QSize(15, 15));
        btn->setFixedHeight(28);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        m_categoryGroup->addButton(btn, i);
        m_catButtons.append(btn);
        catRow->addWidget(btn);
    }

    if (auto* btn = m_categoryGroup->button(0)) {
        btn->setChecked(true);
    }

    rootLayout->addLayout(catRow);

    connect(ThemeManager::instance(), &ThemeManager::themeApplied, this, &HeaderBar::updateStyles);

    Logger::info("HeaderBar: updating styles...");
    updateStyles();
    setFixedHeight(76);
    Logger::info("HeaderBar constructor end");
}

void HeaderBar::updateStyles() {
    bool dark = ThemeManager::instance()->isDarkMode();
    QString primaryHex = ThemeManager::instance()->primaryColor().name();
    QColor topIconCol = dark ? QColor("#c5c5cb") : QColor("#555558");

    m_appIcon->setPixmap(FluentIcon::appIcon(24, true).pixmap(24, 24));
    m_pinBtn->setIcon(FluentIcon::make(FluentIconType::Pin, m_pinBtn->isChecked() ? QColor("#ffffff") : topIconCol, 15));
    m_settingsBtn->setIcon(FluentIcon::make(FluentIconType::Settings, topIconCol, 15));

    // Top action buttons style (Pin, Settings)
    QString actBtnStyle;
    if (dark) {
        actBtnStyle = QString(R"(
            QPushButton {
                border: 1px solid rgba(255, 255, 255, 0.08);
                background: rgba(255, 255, 255, 0.06);
                border-radius: 7px;
                padding: 2px;
            }
            QPushButton:hover {
                background: rgba(255, 255, 255, 0.14);
                border-color: rgba(255, 255, 255, 0.18);
            }
            QPushButton:checked {
                background: %1;
                border-color: %1;
            }
        )").arg(primaryHex);
    } else {
        actBtnStyle = QString(R"(
            QPushButton {
                border: 1px solid rgba(0, 0, 0, 0.07);
                background: rgba(255, 255, 255, 0.70);
                border-radius: 7px;
                padding: 2px;
            }
            QPushButton:hover {
                background: #ffffff;
                border-color: rgba(0, 0, 0, 0.12);
            }
            QPushButton:checked {
                background: %1;
                border-color: %1;
            }
        )").arg(primaryHex);
    }
    m_pinBtn->setStyleSheet(actBtnStyle);
    m_settingsBtn->setStyleSheet(actBtnStyle);

    // Category icon buttons style
    QString catBtnStyle;
    if (dark) {
        catBtnStyle = QString(R"(
            QPushButton {
                border: 1px solid rgba(255, 255, 255, 0.06);
                background: rgba(255, 255, 255, 0.05);
                color: #a1a1a6;
                border-radius: 7px;
                font-size: 13px;
                padding: 2px;
            }
            QPushButton:hover {
                background: rgba(255, 255, 255, 0.12);
                border-color: rgba(255, 255, 255, 0.14);
                color: #e5e5ea;
            }
            QPushButton:checked {
                background: %1;
                color: #ffffff;
                border-color: %1;
            }
        )").arg(primaryHex);
    } else {
        catBtnStyle = QString(R"(
            QPushButton {
                border: 1px solid rgba(0, 0, 0, 0.05);
                background: rgba(255, 255, 255, 0.60);
                color: #555555;
                border-radius: 7px;
                font-size: 13px;
                padding: 2px;
            }
            QPushButton:hover {
                background: #ffffff;
                border-color: rgba(0, 0, 0, 0.10);
                color: #1d1d1f;
            }
            QPushButton:checked {
                background: %1;
                color: #ffffff;
                border-color: %1;
            }
        )").arg(primaryHex);
    }

    QColor catIconCol = dark ? QColor("#c5c5cb") : QColor("#555558");
    if (m_catButtons.size() >= 6) {
        m_catButtons[0]->setIcon(FluentIcon::make(FluentIconType::Reuse, catIconCol, 15));
        m_catButtons[1]->setIcon(FluentIcon::make(FluentIconType::Star, catIconCol, 15));
        m_catButtons[2]->setIcon(FluentIcon::make(FluentIconType::FileText, catIconCol, 15));
        m_catButtons[3]->setIcon(FluentIcon::make(FluentIconType::Image, catIconCol, 15));
        m_catButtons[4]->setIcon(FluentIcon::make(FluentIconType::Folder, catIconCol, 15));
        m_catButtons[5]->setIcon(FluentIcon::make(FluentIconType::Globe, catIconCol, 15));
    }

    for (auto* btn : m_catButtons) {
        btn->setStyleSheet(catBtnStyle);
    }

    if (m_appIcon) {
        m_appIcon->setPixmap(FluentIcon::appIcon(24, true).pixmap(24, 24));
    }
}

void HeaderBar::setRange(const QString& r) {
    if (r == "favorite") {
        if (auto* btn = m_categoryGroup->button(1)) btn->setChecked(true);
        m_range = "favorite";
        m_category = "all";
    } else {
        m_range = "all";
    }
}

void HeaderBar::setCategory(const QString& c) {
    int idx = 0;
    if (c == "text") idx = 2;
    else if (c == "image") idx = 3;
    else if (c == "files") idx = 4;
    else if (c == "link") idx = 5;

    if (auto* btn = m_categoryGroup->button(idx)) {
        btn->setChecked(true);
    }
    m_range = "all";
    m_category = c;
}

void HeaderBar::setPinState(bool pinned) {
    if (m_pinBtn) {
        m_pinBtn->setChecked(pinned);
        bool dark = ThemeManager::instance()->isDarkMode();
        QColor topIconCol = dark ? QColor("#c5c5cb") : QColor("#555558");
        m_pinBtn->setIcon(FluentIcon::make(FluentIconType::Pin, pinned ? QColor("#ffffff") : topIconCol, 15));
    }
}

void HeaderBar::clearSearch() {
    if (m_searchEdit) m_searchEdit->clear();
}

void HeaderBar::focusSearch() {
    if (m_searchEdit) {
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    }
}

void HeaderBar::handleCategoryClicked(int id) {
    switch (id) {
        case 0: // All
            m_range = "all";
            m_category = "all";
            break;
        case 1: // Favorite
            m_range = "favorite";
            m_category = "all";
            break;
        case 2: // Text
            m_range = "all";
            m_category = "text";
            break;
        case 3: // Image
            m_range = "all";
            m_category = "image";
            break;
        case 4: // Files
            m_range = "all";
            m_category = "files";
            break;
        case 5: // Link
            m_range = "all";
            m_category = "link";
            break;
    }
    emit filterChanged();
}

void HeaderBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (window()) {
            HWND hwnd = reinterpret_cast<HWND>(window()->winId());
            ::ReleaseCapture();
            ::SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}
