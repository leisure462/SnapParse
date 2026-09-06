#include "OrganizePage.h"
#include "AppConfig.h"
#include "HistoryCleanService.h"
#include "ThemeManager.h"
#include "Logger.h"

OrganizePage::OrganizePage(QWidget* parent) : QScrollArea(parent) {
    Logger::info("OrganizePage constructor start");
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setupUi();
    showSection(0);
    Logger::info("OrganizePage constructor end");
}

void OrganizePage::setupUi() {
    QWidget* container = new QWidget(this);
    m_contentLayout = new QVBoxLayout(container);
    m_contentLayout->setContentsMargins(20, 16, 20, 24);
    m_contentLayout->setSpacing(18);

    auto cfg = AppConfig::instance();

    // Section 1: History Retention Group
    m_retentionGroup = new FluentSettingCardGroup("保留与清理", container);

    auto* cardRet = new FluentRangeSettingCard(FluentIconType::History, "保留周期", "超过该天数的普通记录会自动清理；收藏和置顶始终保留，0 表示不按时间清理。", this);
    cardRet->setRange(0, 365);
    cardRet->setSuffix(" 天");
    cardRet->setValue(cfg->history.retentionDays);
    connect(cardRet, &FluentRangeSettingCard::valueChanged, this, [](int v) {
        AppConfig::instance()->history.retentionDays = v;
        AppConfig::instance()->save();
        HistoryCleanService::instance()->runCleanup();
    });
    m_rows["history.retention"] = cardRet;
    m_retentionGroup->addSettingCard(cardRet);

    auto* cardMax = new FluentRangeSettingCard(FluentIconType::History, "最大保留条数", "超过上限后，自动清理较旧的普通记录；收藏和置顶始终保留，0 表示不限数量。", this);
    cardMax->setRange(0, 10000);
    cardMax->setSuffix(" 条");
    cardMax->setValue(cfg->history.maxCount);
    connect(cardMax, &FluentRangeSettingCard::valueChanged, this, [](int v) {
        AppConfig::instance()->history.maxCount = v;
        AppConfig::instance()->save();
        HistoryCleanService::instance()->runCleanup();
    });
    m_rows["history.maxCount"] = cardMax;
    m_retentionGroup->addSettingCard(cardMax);

    auto* cardClean = new FluentRangeSettingCard(FluentIconType::History, "自动清理周期", "每隔指定小时检查并清理一次历史记录，0 表示关闭周期性清理；应用启动时仍会清理一次。", this);
    cardClean->setRange(0, 168);
    cardClean->setSuffix(" 小时");
    cardClean->setValue(cfg->history.cleanupIntervalHours);
    connect(cardClean, &FluentRangeSettingCard::valueChanged, this, [](int v) {
        AppConfig::instance()->history.cleanupIntervalHours = v;
        AppConfig::instance()->save();
        HistoryCleanService::instance()->start();
    });
    m_rows["history.cleanupIntervalHours"] = cardClean;
    m_retentionGroup->addSettingCard(cardClean);

    m_contentLayout->addWidget(m_retentionGroup);

    // Section 2: Favorite & Note Group
    m_favGroup = new FluentSettingCardGroup("收藏与备注", container);

    auto* cardFav = new FluentSwitchSettingCard(FluentIconType::Star, "有备注时自动收藏", "保存非空备注时，自动把该记录加入收藏。", this);
    cardFav->setChecked(cfg->content.autoFavorite);
    connect(cardFav, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->content.autoFavorite = chk;
        AppConfig::instance()->save();
    });
    m_rows["organizing.autoFavorite"] = cardFav;
    m_favGroup->addSettingCard(cardFav);

    m_contentLayout->addWidget(m_favGroup);

    // Section 3: Search & Sorting Group
    m_searchGroup = new FluentSettingCardGroup("搜索与排序", container);

    auto* cardSort = new FluentOptionsSettingCard(FluentIconType::History, "默认排序规则", "设置打开剪贴板时记录的默认排列顺序。", this);
    cardSort->setOptions({
        {"updatedAtDesc", "按更新时间"},
        {"createdAtDesc", "按创建时间"},
        {"useCountDesc", "按使用次数"}
    });
    cardSort->setCurrentValue(cfg->content.sort);
    connect(cardSort, &FluentOptionsSettingCard::valueChanged, this, [](const QString& val) {
        AppConfig::instance()->content.sort = val;
        AppConfig::instance()->save();
    });
    m_rows["search.sort"] = cardSort;
    m_searchGroup->addSettingCard(cardSort);

    auto* cardFocus = new FluentSwitchSettingCard(FluentIconType::Search, "打开时聚焦搜索框", "每次打开剪贴板窗口时，默认将焦点置于搜索输入框。", this);
    cardFocus->setChecked(cfg->search.defaultFocus);
    connect(cardFocus, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->search.defaultFocus = chk;
        AppConfig::instance()->save();
    });
    m_rows["search.defaultFocus"] = cardFocus;
    m_searchGroup->addSettingCard(cardFocus);

    auto* cardClear = new FluentSwitchSettingCard(FluentIconType::Search, "隐藏时清空搜索内容", "关闭剪贴板窗口时，自动清空搜索输入框中的关键字。", this);
    cardClear->setChecked(cfg->search.clearOnHide);
    connect(cardClear, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->search.clearOnHide = chk;
        AppConfig::instance()->save();
    });
    m_rows["search.clearOnHide"] = cardClear;
    m_searchGroup->addSettingCard(cardClear);

    m_contentLayout->addWidget(m_searchGroup);
    m_contentLayout->addStretch();

    setWidget(container);
}

void OrganizePage::showSection(int index) {
    if (index == 0) {
        m_retentionGroup->show();
        m_favGroup->hide();
        m_searchGroup->hide();
    } else if (index == 1) {
        m_retentionGroup->hide();
        m_favGroup->show();
        m_searchGroup->hide();
    } else if (index == 2) {
        m_retentionGroup->hide();
        m_favGroup->hide();
        m_searchGroup->show();
    } else {
        m_retentionGroup->show();
        m_favGroup->show();
        m_searchGroup->show();
    }
}

void OrganizePage::scrollToSetting(const QString& settingId) {
    if (settingId.startsWith("history.")) {
        showSection(0);
    } else if (settingId.startsWith("organizing.")) {
        showSection(1);
    } else if (settingId.startsWith("search.")) {
        showSection(2);
    }
    if (m_rows.contains(settingId)) {
        ensureWidgetVisible(m_rows[settingId]);
    }
}
