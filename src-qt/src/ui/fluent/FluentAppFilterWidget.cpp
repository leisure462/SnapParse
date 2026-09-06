#include "FluentAppFilterWidget.h"
#include "SwitchWidget.h"
#include "FluentIcon.h"
#include "ThemeManager.h"
#include "AppPaths.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QPainter>
#include <QFrame>

class FluentAppFilterRow : public QWidget {
    Q_OBJECT
public:
    explicit FluentAppFilterRow(const ClipboardApp& app, bool isIgnored, QWidget* parent = nullptr)
        : QWidget(parent), m_app(app), m_isIgnored(isIgnored) {
        setFixedHeight(52);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(16, 0, 16, 0);
        layout->setSpacing(12);

        // App Icon
        m_iconLabel = new QLabel(this);
        m_iconLabel->setFixedSize(28, 28);
        m_iconLabel->setAlignment(Qt::AlignCenter);
        loadAppIcon();
        layout->addWidget(m_iconLabel);

        // App Info (Name + ID)
        QVBoxLayout* infoLayout = new QVBoxLayout();
        infoLayout->setContentsMargins(0, 0, 0, 0);
        infoLayout->setSpacing(2);
        infoLayout->setAlignment(Qt::AlignVCenter);

        m_nameLabel = new QLabel(app.name.isEmpty() ? app.id : app.name, this);
        m_nameLabel->setStyleSheet("font-size: 13px; font-weight: 500;");
        infoLayout->addWidget(m_nameLabel);

        m_idLabel = new QLabel(app.id, this);
        m_idLabel->setStyleSheet("font-size: 11.5px; color: #86868b;");
        infoLayout->addWidget(m_idLabel);

        layout->addLayout(infoLayout, 1);

        // Status badge / text
        m_statusLabel = new QLabel(this);
        m_statusLabel->setStyleSheet("font-size: 12px; margin-right: 4px;");
        layout->addWidget(m_statusLabel);

        // Toggle Switch (Checked = 采集, Unchecked = 忽略)
        m_switch = new SwitchWidget(this);
        m_switch->setChecked(!isIgnored);
        updateStatusText(!isIgnored);
        connect(m_switch, &SwitchWidget::toggled, this, [this](bool chk) {
            updateStatusText(chk);
            emit toggled(m_app.id, chk);
        });
        layout->addWidget(m_switch);

        // Delete button
        m_deleteBtn = new QPushButton(this);
        m_deleteBtn->setFixedSize(28, 28);
        m_deleteBtn->setCursor(Qt::PointingHandCursor);
        m_deleteBtn->setToolTip("从列表中移除");
        m_deleteBtn->setIcon(FluentIcon::make(FluentIconType::Trash, QColor("#86868b"), 16));
        m_deleteBtn->setStyleSheet("QPushButton { border: none; background: transparent; border-radius: 4px; } QPushButton:hover { background-color: rgba(255, 77, 79, 0.15); }");
        connect(m_deleteBtn, &QPushButton::clicked, this, [this]() {
            emit deleteRequested(m_app.id);
        });
        layout->addWidget(m_deleteBtn);

        connect(ThemeManager::instance(), &ThemeManager::themeApplied, this, &FluentAppFilterRow::updateTheme);
        updateTheme();
    }

    QString appId() const { return m_app.id; }
    QString appName() const { return m_app.name; }
    bool isIgnored() const { return !m_switch->isChecked(); }

signals:
    void toggled(const QString& appId, bool captureEnabled);
    void deleteRequested(const QString& appId);

private:
    void loadAppIcon() {
        QString iconPath = AppPaths::iconCacheDir() + "/" + m_app.iconFile;
        if (!m_app.iconFile.isEmpty() && QFile::exists(iconPath)) {
            QPixmap pix(iconPath);
            if (!pix.isNull()) {
                m_iconLabel->setPixmap(pix.scaled(26, 26, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                return;
            }
        }
        // Fallback Fluent vector icon
        bool dark = ThemeManager::instance()->isDarkMode();
        m_iconLabel->setPixmap(FluentIcon::make(FluentIconType::Workflow, dark ? QColor("#8c8c8c") : QColor("#555555"), 22).pixmap(22, 22));
    }

    void updateStatusText(bool captureEnabled) {
        if (captureEnabled) {
            m_statusLabel->setText("正在采集");
            m_statusLabel->setStyleSheet("font-size: 12px; color: #52c41a; font-weight: 500;");
        } else {
            m_statusLabel->setText("已忽略");
            m_statusLabel->setStyleSheet("font-size: 12px; color: #fa8c16; font-weight: 500;");
        }
    }

    void updateTheme() {
        bool dark = ThemeManager::instance()->isDarkMode();
        if (m_nameLabel) {
            m_nameLabel->setStyleSheet(QString("font-size: 13px; font-weight: 500; color: %1;")
                .arg(dark ? "#f5f5f7" : "#1d1d1f"));
        }
        if (m_idLabel) {
            m_idLabel->setStyleSheet(QString("font-size: 11.5px; color: %1;")
                .arg(dark ? "#8c8c8c" : "#86868b"));
        }
        if (m_deleteBtn) {
            m_deleteBtn->setIcon(FluentIcon::make(FluentIconType::Trash, dark ? QColor("#a1a1a6") : QColor("#86868b"), 16));
        }
        updateStatusText(m_switch ? m_switch->isChecked() : true);
    }

    ClipboardApp m_app;
    bool m_isIgnored;
    QLabel* m_iconLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_idLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    SwitchWidget* m_switch = nullptr;
    QPushButton* m_deleteBtn = nullptr;
};

FluentAppFilterWidget::FluentAppFilterWidget(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(12);

    // ==========================================
    // Top Control Toolbar
    // ==========================================
    QHBoxLayout* topBar = new QHBoxLayout();
    topBar->setContentsMargins(0, 0, 0, 0);
    topBar->setSpacing(10);

    m_searchEdit = new FluentSearchLineEdit(this);
    m_searchEdit->setPlaceholderText("搜索已记录或添加的应用...");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &FluentAppFilterWidget::handleSearchChanged);
    topBar->addWidget(m_searchEdit, 1);

    m_filterSegment = new SegmentedWidget(this);
    m_filterSegment->setOptions({
        {"all", "全部 (0)"},
        {"capture", "采集 (0)"},
        {"ignore", "已忽略 (0)"}
    });
    m_filterSegment->setCurrentValue("all");
    connect(m_filterSegment, &SegmentedWidget::valueChanged, this, &FluentAppFilterWidget::handleFilterTabChanged);
    m_btnRefresh = new QPushButton("扫描运行中进程", this);
    m_btnRefresh->setCursor(Qt::PointingHandCursor);
    m_btnRefresh->setFixedHeight(32);
    m_btnRefresh->setIcon(FluentIcon::make(FluentIconType::Workflow, ThemeManager::instance()->primaryColor(), 15));
    connect(m_btnRefresh, &QPushButton::clicked, this, &FluentAppFilterWidget::handleRefresh);
    connect(ThemeManager::instance(), &ThemeManager::themeApplied, this, [this]() {
        m_btnRefresh->setIcon(FluentIcon::make(FluentIconType::Workflow, ThemeManager::instance()->primaryColor(), 15));
    });
    topBar->addWidget(m_btnRefresh);

    m_btnAddApp = new QPushButton("+ 添加本地应用", this);
    m_btnAddApp->setCursor(Qt::PointingHandCursor);
    m_btnAddApp->setFixedHeight(32);
    connect(m_btnAddApp, &QPushButton::clicked, this, &FluentAppFilterWidget::handleBrowseApp);
    topBar->addWidget(m_btnAddApp);

    rootLayout->addLayout(topBar);

    // ==========================================
    // App List Card Container
    // ==========================================
    m_cardContainer = new QWidget(this);
    m_cardContainer->setObjectName("AppCardContainer");

    m_rowsLayout = new QVBoxLayout(m_cardContainer);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(0);

    m_emptyLabel = new QLabel("暂无已配置的应用过滤项", m_cardContainer);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setFixedHeight(120);
    m_emptyLabel->setStyleSheet("font-size: 13px; color: #86868b;");
    m_rowsLayout->addWidget(m_emptyLabel);

    rootLayout->addWidget(m_cardContainer);

    auto updateContainerStyle = [this]() {
        bool dark = ThemeManager::instance()->isDarkMode();
        QString bg = dark ? "rgba(38, 38, 44, 0.85)" : "rgba(255, 255, 255, 0.9)";
        QString border = dark ? "rgba(255, 255, 255, 0.1)" : "rgba(0, 0, 0, 0.08)";
        m_cardContainer->setStyleSheet(QString(R"(
            QWidget#AppCardContainer {
                background-color: %1;
                border: 1px solid %2;
                border-radius: 10px;
            }
        )").arg(bg, border));

        if (m_btnRefresh) {
            QString refBg = dark ? "rgba(255, 255, 255, 0.07)" : "rgba(0, 0, 0, 0.04)";
            QString refHover = dark ? "rgba(255, 255, 255, 0.12)" : "rgba(0, 0, 0, 0.08)";
            QString refColor = dark ? "#e6e6e6" : "#262626";
            QString refBorder = dark ? "rgba(255, 255, 255, 0.15)" : "rgba(0, 0, 0, 0.12)";
            m_btnRefresh->setStyleSheet(QString(R"(
                QPushButton {
                    background-color: %1;
                    color: %2;
                    border: 1px solid %3;
                    border-radius: 6px;
                    padding: 4px 12px;
                    font-weight: 500;
                    font-size: 12.5px;
                }
                QPushButton:hover {
                    background-color: %4;
                }
            )").arg(refBg, refColor, refBorder, refHover));
        }

        if (m_btnAddApp) {
            m_btnAddApp->setStyleSheet(QString(R"(
                QPushButton {
                    background-color: %1;
                    color: #ffffff;
                    border: none;
                    border-radius: 6px;
                    padding: 4px 14px;
                    font-weight: 500;
                    font-size: 12.5px;
                }
                QPushButton:hover {
                    background-color: #4096ff;
                }
            )").arg(ThemeManager::instance()->primaryColor().name()));
        }
    };

    connect(ThemeManager::instance(), &ThemeManager::themeApplied, this, updateContainerStyle);
    updateContainerStyle();
}

void FluentAppFilterWidget::setAvailableApps(const QList<ClipboardApp>& apps) {
    m_allApps = apps;
    rebuildList();
}

void FluentAppFilterWidget::setExcludedAppIds(const QStringList& excludedIds) {
    m_excludedIds = excludedIds;
    rebuildList();
}

void FluentAppFilterWidget::handleRefresh() {
    emit refreshRequested();
}

void FluentAppFilterWidget::handleSearchChanged(const QString& text) {
    m_currentSearchText = text.trimmed();
    rebuildList();
}

void FluentAppFilterWidget::handleFilterTabChanged(const QString& tab) {
    m_currentFilterTab = tab;
    rebuildList();
}

void FluentAppFilterWidget::handleAppToggled(const QString& appId, bool captureEnabled) {
    if (captureEnabled) {
        m_excludedIds.removeAll(appId);
    } else {
        if (!m_excludedIds.contains(appId)) {
            m_excludedIds.append(appId);
        }
    }
    updateCounters();
    emit excludedAppsChanged(m_excludedIds);
}

void FluentAppFilterWidget::handleAppDeleted(const QString& appId) {
    m_excludedIds.removeAll(appId);
    for (int i = 0; i < m_allApps.size(); ++i) {
        if (m_allApps[i].id == appId) {
            m_allApps.removeAt(i);
            break;
        }
    }
    rebuildList();
    emit excludedAppsChanged(m_excludedIds);
}

void FluentAppFilterWidget::handleBrowseApp() {
    QString exe = QFileDialog::getOpenFileName(this, "选择本地应用程序", "", "Executable (*.exe)");
    if (!exe.isEmpty()) {
        QFileInfo fi(exe);
        QString id = fi.fileName().toLower();
        QString name = fi.baseName();

        ClipboardApp app;
        app.id = id;
        app.name = name;
        app.platform = "windows";

        bool exists = false;
        for (const auto& a : m_allApps) {
            if (a.id.compare(id, Qt::CaseInsensitive) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            m_allApps.prepend(app);
        }

        // Newly added app defaults to ignored or active
        if (!m_excludedIds.contains(id)) {
            m_excludedIds.append(id);
        }

        rebuildList();
        emit excludedAppsChanged(m_excludedIds);
    }
}

void FluentAppFilterWidget::updateCounters() {
    int total = m_allApps.size();
    int ignored = 0;
    for (const auto& app : m_allApps) {
        if (m_excludedIds.contains(app.id, Qt::CaseInsensitive)) {
            ignored++;
        }
    }
    int active = total - ignored;

    m_filterSegment->blockSignals(true);
    m_filterSegment->setOptions({
        {"all", QString("全部 (%1)").arg(total)},
        {"capture", QString("采集 (%1)").arg(active)},
        {"ignore", QString("已忽略 (%1)").arg(ignored)}
    });
    m_filterSegment->setCurrentValue(m_currentFilterTab);
    m_filterSegment->blockSignals(false);
}

void FluentAppFilterWidget::rebuildList() {
    // Clear old row widgets and dividers
    QLayoutItem* item;
    while ((item = m_rowsLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            if (item->widget() == m_emptyLabel) {
                m_emptyLabel->hide();
            } else {
                item->widget()->deleteLater();
            }
        }
        delete item;
    }

    updateCounters();

    bool dark = ThemeManager::instance()->isDarkMode();
    int visibleCount = 0;

    for (const auto& app : m_allApps) {
        bool isIgnored = m_excludedIds.contains(app.id, Qt::CaseInsensitive);

        // Filter tab match
        if (m_currentFilterTab == "capture" && isIgnored) continue;
        if (m_currentFilterTab == "ignore" && !isIgnored) continue;

        // Search text match
        if (!m_currentSearchText.isEmpty()) {
            bool match = app.name.contains(m_currentSearchText, Qt::CaseInsensitive) ||
                         app.id.contains(m_currentSearchText, Qt::CaseInsensitive);
            if (!match) continue;
        }

        if (visibleCount > 0) {
            // Divider line
            QFrame* line = new QFrame(m_cardContainer);
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Plain);
            line->setFixedHeight(1);
            line->setStyleSheet(QString("background-color: %1; border: none;").arg(
                dark ? "rgba(255, 255, 255, 0.08)" : "rgba(0, 0, 0, 0.06)"
            ));
            m_rowsLayout->addWidget(line);
        }

        FluentAppFilterRow* row = new FluentAppFilterRow(app, isIgnored, m_cardContainer);
        connect(row, &FluentAppFilterRow::toggled, this, &FluentAppFilterWidget::handleAppToggled);
        connect(row, &FluentAppFilterRow::deleteRequested, this, &FluentAppFilterWidget::handleAppDeleted);
        m_rowsLayout->addWidget(row);
        visibleCount++;
    }

    if (visibleCount == 0) {
        m_emptyLabel->show();
        m_rowsLayout->addWidget(m_emptyLabel);
    } else {
        m_emptyLabel->hide();
    }
}

#include "FluentAppFilterWidget.moc"
