#include "SegmentedWidget.h"
#include "ThemeManager.h"

static QString getSegmentedButtonStyle() {
    bool dark = ThemeManager::instance()->isDarkMode();
    QString primaryHex = ThemeManager::instance()->primaryColor().name();

    if (dark) {
        return QString(R"(
            QPushButton {
                border: none;
                background: transparent;
                color: #8c8c8c;
                border-radius: 4px;
                padding: 3px 8px;
                font-size: 12px;
            }
            QPushButton:hover {
                color: #e0e0e0;
            }
            QPushButton:checked {
                background: #333333;
                color: %1;
                font-weight: bold;
            }
        )").arg(primaryHex);
    } else {
        return QString(R"(
            QPushButton {
                border: none;
                background: transparent;
                color: #555558;
                border-radius: 4px;
                padding: 3px 8px;
                font-size: 12px;
            }
            QPushButton:hover {
                color: #1d1d1f;
            }
            QPushButton:checked {
                background: #ffffff;
                color: %1;
                font-weight: bold;
            }
        )").arg(primaryHex);
    }
}

SegmentedWidget::SegmentedWidget(QWidget* parent) : QWidget(parent) {
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(2, 2, 2, 2);
    m_layout->setSpacing(2);

    m_buttonGroup = new QButtonGroup(this);
    connect(m_buttonGroup, &QButtonGroup::idClicked, this, &SegmentedWidget::handleButtonClicked);

    setFixedHeight(30);

    connect(ThemeManager::instance(), &ThemeManager::themeApplied, this, [this]() {
        bool dark = ThemeManager::instance()->isDarkMode();
        if (dark) {
            setStyleSheet("background-color: rgba(255, 255, 255, 0.08); border-radius: 6px;");
        } else {
            setStyleSheet("background-color: rgba(0, 0, 0, 0.06); border-radius: 6px;");
        }
        QString btnStyle = getSegmentedButtonStyle();
        for (auto* btn : m_buttonGroup->buttons()) {
            btn->setStyleSheet(btnStyle);
        }
    });

    bool dark = ThemeManager::instance()->isDarkMode();
    if (dark) {
        setStyleSheet("background-color: rgba(255, 255, 255, 0.08); border-radius: 6px;");
    } else {
        setStyleSheet("background-color: rgba(0, 0, 0, 0.06); border-radius: 6px;");
    }
}

void SegmentedWidget::setOptions(const QList<QPair<QString, QString>>& options) {
    m_options = options;

    bool dark = ThemeManager::instance()->isDarkMode();
    if (dark) {
        setStyleSheet("background-color: rgba(255, 255, 255, 0.08); border-radius: 6px;");
    } else {
        setStyleSheet("background-color: rgba(0, 0, 0, 0.06); border-radius: 6px;");
    }

    // Clear old
    QList<QAbstractButton*> oldButtons = m_buttonGroup->buttons();
    for (auto* btn : oldButtons) {
        m_buttonGroup->removeButton(btn);
        m_layout->removeWidget(btn);
        btn->deleteLater();
    }

    QString btnStyle = getSegmentedButtonStyle();

    for (int i = 0; i < options.size(); ++i) {
        QPushButton* btn = new QPushButton(options[i].second, this);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(btnStyle);

        m_buttonGroup->addButton(btn, i);
        m_layout->addWidget(btn);
    }
}

void SegmentedWidget::setCurrentValue(const QString& val) {
    m_currentValue = val;
    for (int i = 0; i < m_options.size(); ++i) {
        if (m_options[i].first == val) {
            QAbstractButton* btn = m_buttonGroup->button(i);
            if (btn) btn->setChecked(true);
            break;
        }
    }
}

void SegmentedWidget::handleButtonClicked(int id) {
    if (id >= 0 && id < m_options.size()) {
        m_currentValue = m_options[id].first;
        emit valueChanged(m_currentValue);
    }
}
