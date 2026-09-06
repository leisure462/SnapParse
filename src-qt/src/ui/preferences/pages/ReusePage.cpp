#include "ReusePage.h"
#include "AppConfig.h"
#include "SortableTreeDialog.h"
#include "ThemeManager.h"
#include "EventBus.h"
#include "Logger.h"

ReusePage::ReusePage(QWidget* parent) : QScrollArea(parent) {
    Logger::info("ReusePage constructor start");
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setupUi();
    showSection(0);
    Logger::info("ReusePage constructor end");
}

void ReusePage::setupUi() {
    QWidget* container = new QWidget(this);
    m_contentLayout = new QVBoxLayout(container);
    m_contentLayout->setContentsMargins(20, 16, 20, 24);
    m_contentLayout->setSpacing(18);

    auto cfg = AppConfig::instance();

    // Section 1: Paste Behavior Group
    m_pasteGroup = new FluentSettingCardGroup("粘贴行为", container);

    auto* cardAutoPaste = new FluentOptionsSettingCard(FluentIconType::Paste, "鼠标左键动作", "选择鼠标左键点击历史记录时执行的动作。", this);
    cardAutoPaste->setOptions({
        {"disabled", "禁用"},
        {"singleClickPaste", "单击粘贴"},
        {"doubleClickPaste", "双击粘贴"},
        {"singleClickCopy", "单击复制"},
        {"doubleClickCopy", "双击复制"}
    });
    cardAutoPaste->setCurrentValue(cfg->content.autoPaste);
    connect(cardAutoPaste, &FluentOptionsSettingCard::valueChanged, this, [](const QString& val) {
        AppConfig::instance()->content.autoPaste = val;
        AppConfig::instance()->save();
    });
    m_rows["paste.autoPaste"] = cardAutoPaste;
    m_pasteGroup->addSettingCard(cardAutoPaste);

    auto* cardMidPaste = new FluentOptionsSettingCard(FluentIconType::Paste, "鼠标中键动作", "选择鼠标中键点击历史记录时执行的动作。", this);
    cardMidPaste->setOptions({
        {"disabled", "禁用"},
        {"singleClickPaste", "粘贴"},
        {"singleClickPastePlain", "纯文本粘贴"},
        {"singleClickCopy", "复制"},
        {"singleClickCopyPlain", "纯文本复制"}
    });
    cardMidPaste->setCurrentValue(cfg->content.middleClick);
    connect(cardMidPaste, &FluentOptionsSettingCard::valueChanged, this, [](const QString& val) {
        AppConfig::instance()->content.middleClick = val;
        AppConfig::instance()->save();
    });
    m_rows["paste.middleClick"] = cardMidPaste;
    m_pasteGroup->addSettingCard(cardMidPaste);

    auto* cardPlainP = new FluentSwitchSettingCard(FluentIconType::FileText, "默认纯文本粘贴", "粘贴文本记录时默认去除 HTML/RTF 等格式，只粘贴纯文本。", this);
    cardPlainP->setChecked(cfg->content.pastePlain);
    connect(cardPlainP, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->content.pastePlain = chk;
        AppConfig::instance()->save();
    });
    m_rows["paste.plainDefault"] = cardPlainP;
    m_pasteGroup->addSettingCard(cardPlainP);

    auto* cardFileP = new FluentSwitchSettingCard(FluentIconType::Folder, "默认文件路径粘贴", "粘贴文件记录时默认不粘贴文件本身，只粘贴文件绝对路径。", this);
    cardFileP->setChecked(cfg->content.pasteFilesAsPath);
    connect(cardFileP, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->content.pasteFilesAsPath = chk;
        AppConfig::instance()->save();
    });
    m_rows["paste.fileMode"] = cardFileP;
    m_pasteGroup->addSettingCard(cardFileP);

    m_contentLayout->addWidget(m_pasteGroup);

    // Section 2: Copy Feedback Group
    m_copyGroup = new FluentSettingCardGroup("复制反馈", container);

    auto* cardPlainC = new FluentSwitchSettingCard(FluentIconType::Copy, "默认复制为纯文本", "复制文本记录时默认去除 HTML/RTF 等格式，只复制纯文本。", this);
    cardPlainC->setChecked(cfg->content.copyPlain);
    connect(cardPlainC, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->content.copyPlain = chk;
        AppConfig::instance()->save();
    });
    m_rows["copy.plainDefault"] = cardPlainC;
    m_copyGroup->addSettingCard(cardPlainC);

    auto* cardHideC = new FluentSwitchSettingCard(FluentIconType::Workflow, "复制后隐藏窗口", "从历史列表复制记录后，自动隐藏剪贴板窗口。", this);
    cardHideC->setChecked(cfg->content.copyThenHideWindow);
    connect(cardHideC, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->content.copyThenHideWindow = chk;
        AppConfig::instance()->save();
    });
    m_rows["copy.hideWindow"] = cardHideC;
    m_copyGroup->addSettingCard(cardHideC);

    auto* cardReuse = new FluentSwitchSettingCard(FluentIconType::History, "使用后更新时间与频次", "复制或粘贴某条记录后，刷新该记录的更新时间并累加使用计数。", this);
    cardReuse->setChecked(cfg->content.updateOnReuse);
    connect(cardReuse, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->content.updateOnReuse = chk;
        AppConfig::instance()->save();
    });
    m_rows["copy.updateOnReuse"] = cardReuse;
    m_copyGroup->addSettingCard(cardReuse);

    auto* cardSound = new FluentSwitchSettingCard(FluentIconType::Record, "记录采集提示音", "成功采集新记录时，播放系统提示音。", this);
    cardSound->setChecked(cfg->feedback.copySound);
    connect(cardSound, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->feedback.copySound = chk;
        AppConfig::instance()->save();
    });
    m_rows["copy.sound"] = cardSound;
    m_copyGroup->addSettingCard(cardSound);

    m_contentLayout->addWidget(m_copyGroup);

    // Section 3: Action Toolbar Management
    m_actionsGroup = new FluentSettingCardGroup("快捷动作", container);

    auto* cardActions = new FluentPushSettingCard(FluentIconType::Settings, "自定义动作", "快捷操作栏按钮", "调整卡片右上角快捷操作栏的按钮显示与排序。", this);
    connect(cardActions, &FluentPushSettingCard::clicked, this, [this]() {
        SortableTreeDialog dlg("自定义快捷操作按钮", true, this);
        auto cfg = AppConfig::instance();
        QMap<QString, QString> itemMap = {
            {"paste", "粘贴"},
            {"pastePlain", "纯文本粘贴"},
            {"copy", "复制"},
            {"copyPlain", "纯文本复制"},
            {"star", "收藏"},
            {"pinItem", "置顶"},
            {"note", "备注"},
            {"delete", "删除"}
        };
        QList<QPair<QString, QString>> items;
        for (const QString& k : cfg->content.itemActionOrder) {
            if (itemMap.contains(k)) {
                items.append({k, itemMap[k]});
                itemMap.remove(k);
            }
        }
        for (auto it = itemMap.begin(); it != itemMap.end(); ++it) {
            items.append({it.key(), it.value()});
        }

        dlg.setItems(items, cfg->content.itemActions);
        if (dlg.exec() == QDialog::Accepted) {
            AppConfig::instance()->content.itemActions = dlg.checkedKeys();
            AppConfig::instance()->content.itemActionOrder = dlg.orderedKeys();
            AppConfig::instance()->save();
            EventBus::instance()->settingsChanged("actions");
        }
    });
    m_rows["actions.quickActions"] = cardActions;
    m_actionsGroup->addSettingCard(cardActions);

    m_contentLayout->addWidget(m_actionsGroup);
    m_contentLayout->addStretch();

    setWidget(container);
}

void ReusePage::showSection(int index) {
    if (index == 0) {
        m_pasteGroup->show();
        m_copyGroup->hide();
        m_actionsGroup->hide();
    } else if (index == 1) {
        m_pasteGroup->hide();
        m_copyGroup->show();
        m_actionsGroup->hide();
    } else if (index == 2) {
        m_pasteGroup->hide();
        m_copyGroup->hide();
        m_actionsGroup->show();
    } else {
        m_pasteGroup->show();
        m_copyGroup->show();
        m_actionsGroup->show();
    }
}

void ReusePage::scrollToSetting(const QString& settingId) {
    if (settingId.startsWith("paste.")) {
        showSection(0);
    } else if (settingId.startsWith("copy.")) {
        showSection(1);
    } else if (settingId.startsWith("actions.")) {
        showSection(2);
    }
    if (m_rows.contains(settingId)) {
        ensureWidgetVisible(m_rows[settingId]);
    }
}
