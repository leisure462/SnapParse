#include "PreferenceHeader.h"
#include "ThemeManager.h"
#include <QPainter>

static QString getHeaderButtonStyle() {
    bool dark = ThemeManager::instance()->isDarkMode();
    QString primaryHex = ThemeManager::instance()->primaryColor().name();
    if (dark) {
        return QString(R"(
            QPushButton {
                border: none;
                background: transparent;
                padding: 4px 10px;
                font-size: 13px;
                font-weight: 500;
                color: #a1a1a6;
                border-radius: 4px;
            }
            QPushButton:hover {
                color: #ffffff;
                background-color: rgba(255, 255, 255, 0.08);
            }
            QPushButton:checked {
                color: %1;
                font-weight: 600;
                border-bottom: 2.5px solid %1;
                border-radius: 0px;
            }
        )").arg(primaryHex);
    } else {
        return QString(R"(
            QPushButton {
                border: none;
                background: transparent;
                padding: 4px 10px;
                font-size: 13px;
                font-weight: 500;
                color: #555558;
                border-radius: 4px;
            }
            QPushButton:hover {
                color: #1d1d1f;
                background-color: rgba(0, 0, 0, 0.05);
            }
            QPushButton:checked {
                color: %1;
                font-weight: 600;
                border-bottom: 2.5px solid %1;
                border-radius: 0px;
            }
        )").arg(primaryHex);
    }
}

PreferenceHeader::PreferenceHeader(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 16, 20, 8);
    mainLayout->setSpacing(10);

    QHBoxLayout* topLayout = new QHBoxLayout();
    m_titleLabel = new QLabel("采集", this);
    m_titleLabel->setStyleSheet("font-weight: 600; font-size: 18px;");
    topLayout->addWidget(m_titleLabel);
    topLayout->addStretch();

    m_searchEdit = new FluentSearchLineEdit(this);
    m_searchEdit->setPlaceholderText("搜索设置项...");
    m_searchEdit->setFixedWidth(220);
    topLayout->addWidget(m_searchEdit);
    mainLayout->addLayout(topLayout);

    // Left-aligned secondary section tab bar
    m_sectionsLayout = new QHBoxLayout();
    m_sectionsLayout->setContentsMargins(0, 0, 0, 0);
    m_sectionsLayout->setSpacing(6);
    m_sectionsLayout->setAlignment(Qt::AlignLeft);

    m_sectionButtons = new QButtonGroup(this);
    connect(m_sectionButtons, &QButtonGroup::idClicked, this, [this](int id) {
        m_lastActiveIndex = id;
        emit sectionSelected(id);
    });

    mainLayout->addLayout(m_sectionsLayout);

    m_searchPopup = new SearchResultPopup(this);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &PreferenceHeader::handleSearchTextChanged);
    connect(m_searchPopup, &SearchResultPopup::itemSelected, this, [this](const QString& tab, const QString& sec, const QString& set) {
        m_searchEdit->clear();
        emit searchResultPicked(tab, sec, set);
    });

    connect(ThemeManager::instance(), &ThemeManager::themeApplied, this, [this]() {
        QString style = getHeaderButtonStyle();
        for (auto* btn : m_sectionButtons->buttons()) {
            btn->setStyleSheet(style);
        }
    });

    setFixedHeight(92);
}

void PreferenceHeader::setTabTitle(const QString& title) {
    m_titleLabel->setText(title);
}

void PreferenceHeader::setSections(const QList<QPair<QString, QString>>& sections, int activeIndex) {
    m_lastSections = sections;
    m_lastActiveIndex = activeIndex;

    // Properly remove all existing items and stretch spacers
    QLayoutItem* item;
    while ((item = m_sectionsLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            m_sectionButtons->removeButton(qobject_cast<QAbstractButton*>(item->widget()));
            item->widget()->deleteLater();
        }
        delete item;
    }

    QString btnStyle = getHeaderButtonStyle();

    for (int i = 0; i < sections.size(); ++i) {
        QPushButton* btn = new QPushButton(sections[i].second, this);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(btnStyle);

        m_sectionButtons->addButton(btn, i);
        m_sectionsLayout->addWidget(btn);
    }
    m_sectionsLayout->addStretch(1);

    if (auto* activeBtn = m_sectionButtons->button(activeIndex)) {
        activeBtn->setChecked(true);
    }
}

void PreferenceHeader::handleSearchTextChanged(const QString& text) {
    if (text.isEmpty()) {
        m_searchPopup->hide();
        return;
    }

    QPoint pos = m_searchEdit->mapToGlobal(QPoint(0, m_searchEdit->height() + 4));
    m_searchPopup->move(pos);
    m_searchPopup->search(text);
}
