#include "PreferenceSidebar.h"
#include "DatabaseManager.h"
#include "ThemeManager.h"
#include "FluentIcon.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>

PreferenceSidebar::PreferenceSidebar(QWidget* parent) : QWidget(parent) {
    setFixedWidth(200);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 16, 10, 16);
    layout->setSpacing(12);

    // App Brand
    QHBoxLayout* brandLayout = new QHBoxLayout();
    brandLayout->setContentsMargins(6, 4, 6, 4);
    brandLayout->setSpacing(10);
    brandLayout->setAlignment(Qt::AlignVCenter);

    QLabel* logoLabel = new QLabel(this);
    logoLabel->setFixedSize(24, 24);
    logoLabel->setScaledContents(true);
    logoLabel->setPixmap(FluentIcon::appIcon(24, true).pixmap(24, 24));

    QLabel* nameLabel = new QLabel("SnapParse", this);
    nameLabel->setStyleSheet("font-weight: 600; font-size: 14.5px;");
    nameLabel->setAlignment(Qt::AlignVCenter);

    brandLayout->addWidget(logoLabel, 0, Qt::AlignVCenter);
    brandLayout->addWidget(nameLabel, 0, Qt::AlignVCenter);
    brandLayout->addStretch();
    layout->addLayout(brandLayout);

    // Nav List (Fluent NavigationInterface style)
    m_navList = new QListWidget(this);
    m_navList->setFrameShape(QFrame::NoFrame);
    m_navList->setIconSize(QSize(18, 18));
    m_navList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    struct TabInfo { FluentIconType icon; QString text; };
    QList<TabInfo> tabs = {
        {FluentIconType::Record, "采集"},
        {FluentIconType::History, "历史"},
        {FluentIconType::Reuse, "操作"},
        {FluentIconType::Workflow, "界面"},
        {FluentIconType::Shortcuts, "快捷键"},
        {FluentIconType::Data, "数据"},
        {FluentIconType::About, "关于"}
    };

    auto updateNavItems = [this, logoLabel, tabs]() {
        bool dark = ThemeManager::instance()->isDarkMode();
        QColor iconCol = dark ? QColor("#d0d0d5") : QColor("#444448");
        QString primaryHex = ThemeManager::instance()->primaryColor().name();

        logoLabel->setPixmap(FluentIcon::appIcon(24, true).pixmap(24, 24));

        int curRow = m_navList->currentRow();
        m_navList->clear();
        for (int i = 0; i < tabs.size(); ++i) {
            QListWidgetItem* item = new QListWidgetItem(FluentIcon::make(tabs[i].icon, iconCol, 18), "  " + tabs[i].text, m_navList);
            item->setData(Qt::UserRole, i);
            item->setSizeHint(QSize(180, 36));
        }

        if (dark) {
            m_navList->setStyleSheet(QString(R"(
                QListWidget {
                    background: transparent;
                    border: none;
                }
                QListWidget::item {
                    padding: 4px 10px;
                    font-size: 13px;
                    font-weight: 500;
                    color: #f5f5f7;
                    border-radius: 6px;
                    margin-bottom: 3px;
                }
                QListWidget::item:hover {
                    background-color: rgba(255, 255, 255, 0.08);
                }
                QListWidget::item:selected {
                    background-color: rgba(255, 255, 255, 0.12);
                    color: %1;
                    font-weight: 600;
                }
            )").arg(primaryHex));
        } else {
            m_navList->setStyleSheet(QString(R"(
                QListWidget {
                    background: transparent;
                    border: none;
                }
                QListWidget::item {
                    padding: 4px 10px;
                    font-size: 13px;
                    font-weight: 500;
                    color: #1d1d1f;
                    border-radius: 6px;
                    margin-bottom: 3px;
                }
                QListWidget::item:hover {
                    background-color: rgba(0, 0, 0, 0.05);
                }
                QListWidget::item:selected {
                    background-color: rgba(0, 0, 0, 0.06);
                    color: %1;
                    font-weight: 600;
                }
            )").arg(primaryHex));
        }

        if (curRow >= 0 && curRow < m_navList->count()) {
            m_navList->setCurrentRow(curRow);
        }
    };

    connect(m_navList, &QListWidget::itemClicked, this, &PreferenceSidebar::handleItemClicked);
    connect(ThemeManager::instance(), &ThemeManager::themeApplied, this, updateNavItems);
    updateNavItems();

    layout->addWidget(m_navList, 1);

    // Storage Meter
    m_storageMeter = new StorageMeterWidget(this);
    layout->addWidget(m_storageMeter);

    m_navList->setCurrentRow(0);
    updateStorageUsage();
}

void PreferenceSidebar::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    bool dark = ThemeManager::instance()->isDarkMode();
    QColor sideBg = dark ? QColor(22, 22, 26, 200) : QColor(245, 245, 248, 200);
    painter.fillRect(rect(), sideBg);
    painter.setPen(QPen(ThemeManager::instance()->glassBorderColor(), 1));
    painter.drawLine(width() - 1, 0, width() - 1, height());
}

void PreferenceSidebar::setCurrentTab(int index) {
    if (index >= 0 && index < m_navList->count()) {
        m_navList->setCurrentRow(index);
    }
}

void PreferenceSidebar::handleItemClicked(QListWidgetItem* item) {
    int index = item->data(Qt::UserRole).toInt();
    emit tabChanged(index);
}

void PreferenceSidebar::updateStorageUsage() {
    auto stats = DatabaseManager::instance()->getStorageUsage();
    qint64 totalBytes = stats.totalBytes;
    // Standard storage capacity: 1 GB
    qint64 maxCapacity = 1024ULL * 1024 * 1024;
    m_storageMeter->setUsage(totalBytes, maxCapacity);
}
