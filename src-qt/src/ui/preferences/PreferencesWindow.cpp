#include "PreferencesWindow.h"
#include "ThemeManager.h"
#include "MemoryManager.h"
#include "FluentIcon.h"
#include "Logger.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QEasingCurve>
#include <windows.h>

PreferencesWindow::PreferencesWindow(QWidget* parent) : QWidget(parent) {
    Logger::info("PreferencesWindow constructor start");
    setWindowTitle("偏好设置 - SnapParse");
    setWindowIcon(FluentIcon::appIcon(32));
    resize(900, 620);
    setMinimumSize(820, 560);

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Left Sidebar
    m_sidebar = new PreferenceSidebar(this);
    connect(m_sidebar, &PreferenceSidebar::tabChanged, this, &PreferencesWindow::handleTabChanged);
    mainLayout->addWidget(m_sidebar);

    // Right Content Area
    QWidget* rightArea = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightArea);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    m_header = new PreferenceHeader(this);
    connect(m_header, &PreferenceHeader::sectionSelected, this, &PreferencesWindow::handleSectionSelected);
    connect(m_header, &PreferenceHeader::searchResultPicked, this, &PreferencesWindow::handleSearchResultPicked);
    rightLayout->addWidget(m_header);

    m_stack = new QStackedWidget(this);
    m_pageRecord = new RecordPage(this);
    m_pageOrganize = new OrganizePage(this);
    m_pageReuse = new ReusePage(this);
    m_pageWorkflow = new WorkflowPage(this);
    m_pageShortcuts = new ShortcutsPage(this);
    m_pageData = new DataPage(this);
    m_pageAbout = new AboutPage(this);

    m_stack->addWidget(m_pageRecord);
    m_stack->addWidget(m_pageOrganize);
    m_stack->addWidget(m_pageReuse);
    m_stack->addWidget(m_pageWorkflow);
    m_stack->addWidget(m_pageShortcuts);
    m_stack->addWidget(m_pageData);
    m_stack->addWidget(m_pageAbout);

    rightLayout->addWidget(m_stack, 1);
    mainLayout->addWidget(rightArea, 1);

    connect(ThemeManager::instance(), &ThemeManager::themeApplied, this, [this]() {
        update();
    });

    updateHeaderForTab(0);
    handleSectionSelected(0);
    Logger::info("PreferencesWindow constructor end");
}

void PreferencesWindow::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.fillRect(rect(), ThemeManager::instance()->backgroundColor());
}

void PreferencesWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    MemoryManager::instance()->notifyWindowShown();

    QGraphicsOpacityEffect* eff = new QGraphicsOpacityEffect(this);
    this->setGraphicsEffect(eff);
    QPropertyAnimation* anim = new QPropertyAnimation(eff, "opacity", this);
    anim->setDuration(200);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutQuad);
    connect(anim, &QPropertyAnimation::finished, [this]() {
        this->setGraphicsEffect(nullptr);
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void PreferencesWindow::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    MemoryManager::instance()->notifyWindowHidden();
}

void PreferencesWindow::handleTabChanged(int index) {
    QWidget* nextWidget = m_stack->widget(index);
    if (nextWidget && m_stack->currentIndex() != index) {
        QGraphicsOpacityEffect* eff = new QGraphicsOpacityEffect(nextWidget);
        nextWidget->setGraphicsEffect(eff);
        m_stack->setCurrentIndex(index);

        QPropertyAnimation* anim = new QPropertyAnimation(eff, "opacity", nextWidget);
        anim->setDuration(150);
        anim->setStartValue(0.2);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim, &QPropertyAnimation::finished, [nextWidget, eff]() {
            nextWidget->setGraphicsEffect(nullptr);
        });
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        m_stack->setCurrentIndex(index);
    }
    updateHeaderForTab(index);
    handleSectionSelected(0);
}

void PreferencesWindow::handleSectionSelected(int index) {
    int currentTab = m_stack->currentIndex();
    switch (currentTab) {
        case 0: m_pageRecord->showSection(index); break;
        case 1: m_pageOrganize->showSection(index); break;
        case 2: m_pageReuse->showSection(index); break;
        case 3: m_pageWorkflow->showSection(index); break;
        case 4: m_pageShortcuts->showSection(index); break;
        case 5: m_pageData->showSection(index); break;
        case 6: m_pageAbout->showSection(index); break;
    }
}

void PreferencesWindow::updateHeaderForTab(int index) {
    switch (index) {
        case 0:
            m_header->setTabTitle("采集");
            m_header->setSections({{"capture", "内容类型"}, {"source", "应用过滤"}, {"sensitive", "隐私保护"}});
            break;
        case 1:
            m_header->setTabTitle("历史");
            m_header->setSections({{"history", "保留与清理"}, {"organizing", "收藏与备注"}, {"search", "搜索与排序"}});
            break;
        case 2:
            m_header->setTabTitle("操作");
            m_header->setSections({{"paste", "粘贴行为"}, {"copy", "复制反馈"}, {"actions", "快捷动作"}});
            break;
        case 3:
            m_header->setTabTitle("界面");
            m_header->setSections({{"window", "剪贴板窗口"}, {"preview", "内容预览"}, {"appearance", "外观与语言"}, {"control", "系统入口"}});
            break;
        case 4:
            m_header->setTabTitle("快捷键");
            m_header->setSections({{"globalShortcuts", "全局快捷键"}});
            break;
        case 5:
            m_header->setTabTitle("数据");
            m_header->setSections({{"localData", "存储位置"}, {"backup", "备份迁移"}, {"diagnostics", "诊断恢复"}, {"updates", "更新"}});
            break;
        case 6:
            m_header->setTabTitle("关于");
            m_header->setSections({{"about", "应用信息"}});
            break;
    }
}

void PreferencesWindow::handleSearchResultPicked(const QString& tabId, const QString& sectionId, const QString& settingId) {
    Q_UNUSED(sectionId)
    int tabIndex = 0;
    if (tabId == "record") tabIndex = 0;
    else if (tabId == "organize") tabIndex = 1;
    else if (tabId == "reuse") tabIndex = 2;
    else if (tabId == "workflow") tabIndex = 3;
    else if (tabId == "shortcuts") tabIndex = 4;
    else if (tabId == "data") tabIndex = 5;
    else if (tabId == "about") tabIndex = 6;

    m_sidebar->setCurrentTab(tabIndex);
    m_stack->setCurrentIndex(tabIndex);
    updateHeaderForTab(tabIndex);

    // Scroll to row
    switch (tabIndex) {
        case 0: m_pageRecord->scrollToSetting(settingId); break;
        case 1: m_pageOrganize->scrollToSetting(settingId); break;
        case 2: m_pageReuse->scrollToSetting(settingId); break;
        case 3: m_pageWorkflow->scrollToSetting(settingId); break;
        case 4: m_pageShortcuts->scrollToSetting(settingId); break;
        case 5: m_pageData->scrollToSetting(settingId); break;
        case 6: m_pageAbout->scrollToSetting(settingId); break;
    }
}

void PreferencesWindow::showAndFocus(const QString& targetTab, const QString& targetSettingId) {
    m_sidebar->updateStorageUsage();
    show();
    raise();
    activateWindow();
    SetForegroundWindow(reinterpret_cast<HWND>(winId()));

    if (!targetTab.isEmpty()) {
        handleSearchResultPicked(targetTab, "", targetSettingId);
    }
}
