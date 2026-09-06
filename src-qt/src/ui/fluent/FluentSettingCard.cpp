#include "FluentSettingCard.h"
#include "ThemeManager.h"
#include <QPainter>
#include <QPainterPath>

FluentSettingCard::FluentSettingCard(const QString& title, const QString& content, QWidget* parent)
    : QWidget(parent), m_hasIcon(false) {
    initUi(title, content);
}

FluentSettingCard::FluentSettingCard(FluentIconType icon, const QString& title, const QString& content, QWidget* parent)
    : QWidget(parent), m_iconType(icon), m_hasIcon(true) {
    initUi(title, content);
    setIcon(icon);
}

void FluentSettingCard::initUi(const QString& title, const QString& content) {
    setMinimumHeight(64);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(18, 10, 18, 10);
    m_mainLayout->setSpacing(14);

    if (m_hasIcon) {
        m_iconLabel = new QLabel(this);
        m_iconLabel->setFixedSize(22, 22);
        m_iconLabel->setAlignment(Qt::AlignCenter);
        m_mainLayout->addWidget(m_iconLabel, 0, Qt::AlignVCenter);
    }

    QVBoxLayout* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(3);
    textLayout->setAlignment(Qt::AlignVCenter);

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    textLayout->addWidget(m_titleLabel);

    if (!content.isEmpty()) {
        m_contentLabel = new QLabel(content, this);
        m_contentLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        m_contentLabel->setWordWrap(true);
        textLayout->addWidget(m_contentLabel);
    }

    m_mainLayout->addLayout(textLayout, 1);

    auto updateThemeColors = [this]() {
        bool dark = ThemeManager::instance()->isDarkMode();
        if (m_titleLabel) {
            m_titleLabel->setStyleSheet(QString("font-family: 'Microsoft YaHei UI'; font-size: 13px; font-weight: 500; color: %1;")
                .arg(dark ? "#f5f5f7" : "#1d1d1f"));
        }
        if (m_contentLabel) {
            m_contentLabel->setStyleSheet(QString("font-family: 'Microsoft YaHei UI'; font-size: 12px; font-weight: 400; color: %1; line-height: 1.3;")
                .arg(dark ? "#98989f" : "#86868b"));
        }
        if (m_hasIcon && m_iconLabel) {
            setIcon(m_iconType);
        }
    };

    connect(ThemeManager::instance(), &ThemeManager::themeApplied, this, updateThemeColors);
    updateThemeColors();
}

void FluentSettingCard::setTitle(const QString& title) {
    if (m_titleLabel) m_titleLabel->setText(title);
}

void FluentSettingCard::setContent(const QString& content) {
    if (m_contentLabel) {
        m_contentLabel->setText(content);
        m_contentLabel->setVisible(!content.isEmpty());
    }
}

void FluentSettingCard::setIcon(FluentIconType icon) {
    m_iconType = icon;
    m_hasIcon = true;
    if (!m_iconLabel) {
        m_iconLabel = new QLabel(this);
        m_iconLabel->setFixedSize(22, 22);
        m_iconLabel->setAlignment(Qt::AlignCenter);
        m_mainLayout->insertWidget(0, m_iconLabel, 0, Qt::AlignVCenter);
    }
    bool dark = ThemeManager::instance()->isDarkMode();
    QColor col = dark ? QColor("#d0d0d5") : QColor("#444448");
    m_iconLabel->setPixmap(FluentIcon::make(icon, col, 20).pixmap(20, 20));
}

void FluentSettingCard::setTrailingWidget(QWidget* widget) {
    if (m_trailingWidget) {
        m_mainLayout->removeWidget(m_trailingWidget);
    }
    m_trailingWidget = widget;
    if (widget) {
        widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        m_mainLayout->addWidget(widget, 0, Qt::AlignRight | Qt::AlignVCenter);
    }
}

void FluentSettingCard::setCardEnabled(bool enabled) {
    setEnabled(enabled);
    if (m_titleLabel) m_titleLabel->setEnabled(enabled);
    if (m_contentLabel) m_contentLabel->setEnabled(enabled);
    if (m_trailingWidget) m_trailingWidget->setEnabled(enabled);
}

void FluentSettingCard::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event)
    m_isHovered = true;
    update();
}

void FluentSettingCard::leaveEvent(QEvent* event) {
    Q_UNUSED(event)
    m_isHovered = false;
    update();
}

void FluentSettingCard::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Subtle row hover state
    if (m_isHovered && isEnabled()) {
        bool dark = ThemeManager::instance()->isDarkMode();
        QColor hoverBg = dark ? QColor(255, 255, 255, 10) : QColor(0, 0, 0, 8);
        painter.fillRect(rect(), hoverBg);
    }
}
