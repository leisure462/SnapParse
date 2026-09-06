#include "WorkflowPage.h"
#include "AppConfig.h"
#include "AutoStartManager.h"
#include "SystemTrayManager.h"
#include "ThemeManager.h"
#include "ClipboardCardDelegate.h"
#include "EventBus.h"
#include "FluentColorSettingCard.h"
#include "FluentFontSettingCard.h"
#include "Logger.h"

WorkflowPage::WorkflowPage(QWidget* parent) : QScrollArea(parent) {
    Logger::info("WorkflowPage constructor start");
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setupUi();
    showSection(0);
    Logger::info("WorkflowPage constructor end");
}

void WorkflowPage::setupUi() {
    QWidget* container = new QWidget(this);
    m_contentLayout = new QVBoxLayout(container);
    m_contentLayout->setContentsMargins(20, 16, 20, 24);
    m_contentLayout->setSpacing(18);

    auto cfg = AppConfig::instance();

    // Section 1: Window Position Group
    m_winGroup = new FluentSettingCardGroup("剪贴板窗口", container);

    auto* cardPos = new FluentOptionsSettingCard(FluentIconType::Workflow, "打开位置", "选择每次打开剪贴板窗口时的定位方式。", this);
    cardPos->setOptions({
        {"followCursor", "跟随光标"},
        {"center", "屏幕中间"},
        {"remember", "记住位置"}
    });
    cardPos->setCurrentValue(cfg->window.position);
    connect(cardPos, &FluentOptionsSettingCard::valueChanged, this, [](const QString& val) {
        AppConfig::instance()->window.position = val;
        AppConfig::instance()->save();
    });
    m_rows["window.position"] = cardPos;
    m_winGroup->addSettingCard(cardPos);

    auto* cardTop = new FluentSwitchSettingCard(FluentIconType::Workflow, "打开时回到顶部", "每次打开剪贴板窗口时，自动回到最新记录所在的顶部。", this);
    cardTop->setChecked(cfg->window.scrollToTopOnOpen);
    connect(cardTop, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->window.scrollToTopOnOpen = chk;
        AppConfig::instance()->save();
    });
    m_rows["window.scrollToTopOnOpen"] = cardTop;
    m_winGroup->addSettingCard(cardTop);

    m_contentLayout->addWidget(m_winGroup);

    // Section 2: Preview Group
    m_prvGroup = new FluentSettingCardGroup("内容预览", container);

    auto* cardHov = new FluentSwitchSettingCard(FluentIconType::About, "悬停预览", "鼠标悬停在记录上时显示完整内容预览窗口。", this);
    cardHov->setChecked(cfg->preview.hoverEnabled);

    auto* cardDelay = new FluentOptionsSettingCard(FluentIconType::History, "悬停延迟", "设置鼠标悬停多久后触发显示预览。", this);
    cardDelay->setOptions({
        {"300", "300ms"},
        {"500", "500ms"},
        {"1000", "1000ms"}
    });
    cardDelay->setCurrentValue(QString::number(cfg->preview.hoverDelayMs));
    connect(cardDelay, &FluentOptionsSettingCard::valueChanged, this, [](const QString& val) {
        AppConfig::instance()->preview.hoverDelayMs = val.toInt();
        AppConfig::instance()->save();
    });
    m_rows["preview.delay"] = cardDelay;

    connect(cardHov, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->preview.hoverEnabled = chk;
        AppConfig::instance()->save();
    });
    m_rows["preview.hover"] = cardHov;
    m_prvGroup->addSettingCard(cardHov);
    m_prvGroup->addSettingCard(cardDelay);

    auto* cardSpc = new FluentSwitchSettingCard(FluentIconType::Shortcuts, "空格键快速预览", "按空格键即时弹出或关闭当前选中记录的预览窗口。", this);
    cardSpc->setChecked(cfg->preview.spaceEnabled);
    connect(cardSpc, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->preview.spaceEnabled = chk;
        AppConfig::instance()->save();
    });
    m_rows["preview.space"] = cardSpc;
    m_prvGroup->addSettingCard(cardSpc);

    m_contentLayout->addWidget(m_prvGroup);

    // Section 3: Appearance Group
    m_appGroup = new FluentSettingCardGroup("外观与语言", container);

    auto* cardTheme = new FluentOptionsSettingCard(FluentIconType::Settings, "主题外观", "选择浅色、深色或跟随系统外观。", this);
    cardTheme->setOptions({
        {"auto", "跟随系统"},
        {"light", "浅色模式"},
        {"dark", "深色模式"}
    });
    cardTheme->setCurrentValue(cfg->appearance.theme);
    connect(cardTheme, &FluentOptionsSettingCard::valueChanged, this, [](const QString& val) {
        AppConfig::instance()->appearance.theme = val;
        AppConfig::instance()->save();
        ThemeManager::instance()->applyTheme(val);
    });
    m_rows["appearance.theme"] = cardTheme;
    m_appGroup->addSettingCard(cardTheme);

    auto* cardColor = new FluentColorSettingCard(FluentIconType::Shield, "主题强调色", "选择软件主色调，界面图标与高亮将实时切换。", this);
    cardColor->setCurrentColor(cfg->appearance.accentColor);
    connect(cardColor, &FluentColorSettingCard::colorChanged, this, [](const QString& hex) {
        ThemeManager::instance()->setAccentColor(hex);
    });
    m_rows["appearance.accentColor"] = cardColor;
    m_appGroup->addSettingCard(cardColor);

    auto* cardFont = new FluentFontSettingCard(FluentIconType::Note, "界面字体", "自定义软件界面显示的字体，支持检测所有本机已安装字体。", this);
    cardFont->setCurrentFont(cfg->appearance.fontFamily);
    connect(cardFont, &FluentFontSettingCard::fontChanged, this, [](const QString& fontFam) {
        ThemeManager::instance()->setFontFamily(fontFam);
    });
    m_rows["appearance.fontFamily"] = cardFont;
    m_appGroup->addSettingCard(cardFont);

    auto* cardLang = new FluentOptionsSettingCard(FluentIconType::Globe, "界面语言", "切换软件界面显示语言。", this);
    cardLang->setOptions({
        {"zh-CN", "简体中文"},
        {"en-US", "English"}
    });
    cardLang->setCurrentValue(cfg->appearance.language);
    connect(cardLang, &FluentOptionsSettingCard::valueChanged, this, [](const QString& val) {
        AppConfig::instance()->appearance.language = val;
        AppConfig::instance()->save();
    });
    m_rows["appearance.language"] = cardLang;
    m_appGroup->addSettingCard(cardLang);

    auto* cardLines = new FluentRangeSettingCard(FluentIconType::FileText, "文本卡片最大行数", "限制剪贴板列表中每条纯文本卡片显示的最大行数。", this);
    cardLines->setRange(1, 5);
    cardLines->setSuffix(" 行");
    cardLines->setValue(cfg->display.textMaxLines);
    connect(cardLines, &FluentRangeSettingCard::valueChanged, this, [](int v) {
        AppConfig::instance()->display.textMaxLines = v;
        AppConfig::instance()->save();
        EventBus::instance()->clipboardUpdated();
    });
    m_rows["display.textMaxLines"] = cardLines;
    m_appGroup->addSettingCard(cardLines);

    auto* cardImgH = new FluentRangeSettingCard(FluentIconType::Image, "图片缩略图高度", "限制剪贴板列表中图片卡片缩略图的最大高度。", this);
    cardImgH->setRange(32, 160);
    cardImgH->setSuffix(" px");
    cardImgH->setValue(cfg->display.imageMaxHeight);
    connect(cardImgH, &FluentRangeSettingCard::valueChanged, this, [](int v) {
        AppConfig::instance()->display.imageMaxHeight = v;
        AppConfig::instance()->save();
        ClipboardCardDelegate::clearThumbnailCache();
        EventBus::instance()->clipboardUpdated();
    });
    m_rows["display.imageMaxHeight"] = cardImgH;
    m_appGroup->addSettingCard(cardImgH);

    m_contentLayout->addWidget(m_appGroup);

    // Section 4: System Control Group
    m_ctrlGroup = new FluentSettingCardGroup("系统入口", container);

    auto* cardAuto = new FluentSwitchSettingCard(FluentIconType::Workflow, "开机自动启动", "登录 Windows 系统后自动在后台启动 SnapParse。", this);
    cardAuto->setChecked(AutoStartManager::isAutoStartEnabled());
    connect(cardAuto, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AutoStartManager::setAutoStartEnabled(chk);
        AppConfig::instance()->general.autoStart = chk;
        AppConfig::instance()->save();
    });
    m_rows["control.autoStart"] = cardAuto;
    m_ctrlGroup->addSettingCard(cardAuto);

    auto* cardTray = new FluentSwitchSettingCard(FluentIconType::Workflow, "系统托盘图标", "在任务栏右下角系统托盘显示 SnapParse 图标。", this);
    cardTray->setChecked(cfg->general.trayIcon);
    connect(cardTray, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->general.trayIcon = chk;
        AppConfig::instance()->save();
        SystemTrayManager::instance()->setVisible(chk);
    });
    m_rows["control.trayIcon"] = cardTray;
    m_ctrlGroup->addSettingCard(cardTray);

    m_contentLayout->addWidget(m_ctrlGroup);
    m_contentLayout->addStretch();

    setWidget(container);
}

void WorkflowPage::showSection(int index) {
    if (index == 0) {
        m_winGroup->show();
        m_prvGroup->hide();
        m_appGroup->hide();
        m_ctrlGroup->hide();
    } else if (index == 1) {
        m_winGroup->hide();
        m_prvGroup->show();
        m_appGroup->hide();
        m_ctrlGroup->hide();
    } else if (index == 2) {
        m_winGroup->hide();
        m_prvGroup->hide();
        m_appGroup->show();
        m_ctrlGroup->hide();
    } else if (index == 3) {
        m_winGroup->hide();
        m_prvGroup->hide();
        m_appGroup->hide();
        m_ctrlGroup->show();
    } else {
        m_winGroup->show();
        m_prvGroup->show();
        m_appGroup->show();
        m_ctrlGroup->show();
    }
}

void WorkflowPage::scrollToSetting(const QString& settingId) {
    if (settingId.startsWith("window.")) {
        showSection(0);
    } else if (settingId.startsWith("preview.")) {
        showSection(1);
    } else if (settingId.startsWith("appearance.") || settingId.startsWith("display.")) {
        showSection(2);
    } else if (settingId.startsWith("control.")) {
        showSection(3);
    }
    if (m_rows.contains(settingId)) {
        ensureWidgetVisible(m_rows[settingId]);
    }
}
