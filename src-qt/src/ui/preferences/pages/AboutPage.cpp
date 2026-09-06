#include "AboutPage.h"
#include "ThemeManager.h"
#include <QDesktopServices>
#include <QUrl>

AboutPage::AboutPage(QWidget* parent) : QScrollArea(parent) {
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setupUi();
    showSection(0);
}

void AboutPage::setupUi() {
    QWidget* container = new QWidget(this);
    m_contentLayout = new QVBoxLayout(container);
    m_contentLayout->setContentsMargins(20, 16, 20, 24);
    m_contentLayout->setSpacing(18);

    m_abtGroup = new FluentSettingCardGroup("关于 SnapParse", container);

    auto* cardVer = new FluentPushSettingCard(FluentIconType::About, "检查更新", "SnapParse Qt", "当前版本 v3.0.0 (Qt 6.8.2 C++20 x64)", this);
    connect(cardVer, &FluentPushSettingCard::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/leisure462/SnapParse/releases"));
    });
    m_rows["about.version"] = cardVer;
    m_abtGroup->addSettingCard(cardVer);

    auto* cardGit = new FluentPushSettingCard(FluentIconType::Globe, "访问主页", "开源项目", "基于 Fluent Design 规范的现代化剪贴板管理工具。", this);
    connect(cardGit, &FluentPushSettingCard::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/EcoPasteHub/EcoPaste"));
    });
    m_rows["about.github"] = cardGit;
    m_abtGroup->addSettingCard(cardGit);

    m_contentLayout->addWidget(m_abtGroup);
    m_contentLayout->addStretch();

    setWidget(container);
}

void AboutPage::showSection(int index) {
    Q_UNUSED(index)
    m_abtGroup->show();
}

void AboutPage::scrollToSetting(const QString& settingId) {
    showSection(0);
    if (m_rows.contains(settingId)) {
        ensureWidgetVisible(m_rows[settingId]);
    }
}
