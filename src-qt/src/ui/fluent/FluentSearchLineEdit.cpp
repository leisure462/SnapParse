#include "FluentSearchLineEdit.h"
#include "ThemeManager.h"
#include <QHBoxLayout>

FluentSearchLineEdit::FluentSearchLineEdit(QWidget* parent) : QLineEdit(parent) {
    setFixedHeight(30);

    m_searchIcon = new QLabel(this);
    m_searchIcon->setFixedSize(16, 16);

    m_clearBtn = new QPushButton("×", this);
    m_clearBtn->setFixedSize(18, 18);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->hide();

    connect(m_clearBtn, &QPushButton::clicked, this, &QLineEdit::clear);
    connect(this, &QLineEdit::textChanged, this, [this](const QString& txt) {
        m_clearBtn->setVisible(!txt.isEmpty());
    });

    connect(ThemeManager::instance(), &ThemeManager::themeApplied, this, &FluentSearchLineEdit::updateStyles);
    updateStyles();
}

void FluentSearchLineEdit::setPlaceholderText(const QString& text) {
    QLineEdit::setPlaceholderText(text);
}

void FluentSearchLineEdit::resizeEvent(QResizeEvent* event) {
    QLineEdit::resizeEvent(event);
    m_searchIcon->move(8, (height() - 16) / 2);
    m_clearBtn->move(width() - 24, (height() - 18) / 2);
}

void FluentSearchLineEdit::updateStyles() {
    bool dark = ThemeManager::instance()->isDarkMode();
    QColor iconCol = dark ? QColor("#98989f") : QColor("#86868b");
    m_searchIcon->setPixmap(FluentIcon::make(FluentIconType::Search, iconCol, 16).pixmap(16, 16));

    QString primaryHex = ThemeManager::instance()->primaryColor().name();

    if (dark) {
        setStyleSheet(QString(R"(
            QLineEdit {
                background-color: rgba(255, 255, 255, 0.07);
                border: 1px solid rgba(255, 255, 255, 0.12);
                border-radius: 7px;
                padding-left: 28px;
                padding-right: 28px;
                font-family: 'Microsoft YaHei UI';
                font-size: 12.5px;
                color: #f5f5f7;
                selection-background-color: %1;
            }
            QLineEdit:focus {
                background-color: rgba(255, 255, 255, 0.10);
                border: 1.5px solid %1;
            }
        )").arg(primaryHex));
        m_clearBtn->setStyleSheet("QPushButton { border: none; background: transparent; color: #a1a1a6; font-size: 14px; font-weight: bold; border-radius: 9px; } QPushButton:hover { background: rgba(255,255,255,0.15); color: #ffffff; }");
    } else {
        setStyleSheet(QString(R"(
            QLineEdit {
                background-color: rgba(255, 255, 255, 0.85);
                border: 1px solid rgba(0, 0, 0, 0.10);
                border-radius: 7px;
                padding-left: 28px;
                padding-right: 28px;
                font-family: 'Microsoft YaHei UI';
                font-size: 12.5px;
                color: #1d1d1f;
                selection-background-color: %1;
            }
            QLineEdit:focus {
                background-color: #ffffff;
                border: 1.5px solid %1;
            }
        )").arg(primaryHex));
        m_clearBtn->setStyleSheet("QPushButton { border: none; background: transparent; color: #86868b; font-size: 14px; font-weight: bold; border-radius: 9px; } QPushButton:hover { background: rgba(0,0,0,0.1); color: #1d1d1f; }");
    }
}
