#include "ShortcutsPage.h"
#include "AppConfig.h"
#include "KeySequenceRecorder.h"
#include "HotkeyManager.h"
#include "ThemeManager.h"
#include <QMessageBox>

ShortcutsPage::ShortcutsPage(QWidget* parent) : QScrollArea(parent) {
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setupUi();
    showSection(0);
}

void ShortcutsPage::setupUi() {
    QWidget* container = new QWidget(this);
    m_contentLayout = new QVBoxLayout(container);
    m_contentLayout->setContentsMargins(20, 16, 20, 24);
    m_contentLayout->setSpacing(18);

    auto cfg = AppConfig::instance();

    m_scGroup = new FluentSettingCardGroup("全局快捷键", container);

    // Open Clipboard Shortcut
    KeySequenceRecorder* recClip = new KeySequenceRecorder(this);
    recClip->setKeySequence(cfg->shortcuts.openClipboard);
    connect(recClip, &KeySequenceRecorder::sequenceChanged, this, [this, recClip](const QString& seq) {
        if (!seq.isEmpty() && seq == AppConfig::instance()->shortcuts.openPreference) {
            QMessageBox::warning(this, "快捷键冲突", "此快捷键已分配给“打开偏好设置窗口”");
            recClip->setKeySequence(AppConfig::instance()->shortcuts.openClipboard);
            return;
        }
        AppConfig::instance()->shortcuts.openClipboard = seq;
        AppConfig::instance()->save();
        HotkeyManager::instance()->registerHotkeys();
    });

    FluentSettingCard* cardClip = new FluentSettingCard(FluentIconType::Shortcuts, "打开剪贴板窗口", "全局呼出或隐藏剪贴板窗口的主快捷键。", this);
    cardClip->setTrailingWidget(recClip);
    m_rows["shortcuts.openClipboard"] = cardClip;
    m_scGroup->addSettingCard(cardClip);

    // Open Preference Shortcut
    KeySequenceRecorder* recPref = new KeySequenceRecorder(this);
    recPref->setKeySequence(cfg->shortcuts.openPreference);
    connect(recPref, &KeySequenceRecorder::sequenceChanged, this, [this, recPref](const QString& seq) {
        if (!seq.isEmpty() && seq == AppConfig::instance()->shortcuts.openClipboard) {
            QMessageBox::warning(this, "快捷键冲突", "此快捷键已分配给“打开剪贴板窗口”");
            recPref->setKeySequence(AppConfig::instance()->shortcuts.openPreference);
            return;
        }
        AppConfig::instance()->shortcuts.openPreference = seq;
        AppConfig::instance()->save();
        HotkeyManager::instance()->registerHotkeys();
    });

    FluentSettingCard* cardPref = new FluentSettingCard(FluentIconType::Settings, "打开偏好设置窗口", "全局呼出偏好设置配置窗口的快捷键。", this);
    cardPref->setTrailingWidget(recPref);
    m_rows["shortcuts.openPreference"] = cardPref;
    m_scGroup->addSettingCard(cardPref);

    // Win+V Hook Shortcut
    auto* cardWinV = new FluentSwitchSettingCard(FluentIconType::Shortcuts, "快捷键 Win+V 唤起 SnapParse", "开启后，按 Win+V 将打开 SnapParse 而不是 Windows 自带剪贴板面板。", this);
    cardWinV->setChecked(cfg->shortcuts.winV);
    connect(cardWinV, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->shortcuts.winV = chk;
        AppConfig::instance()->save();
    });
    m_rows["shortcuts.winV"] = cardWinV;
    m_scGroup->addSettingCard(cardWinV);

    m_contentLayout->addWidget(m_scGroup);
    m_contentLayout->addStretch();

    setWidget(container);
}

void ShortcutsPage::showSection(int index) {
    Q_UNUSED(index)
    m_scGroup->show();
}

void ShortcutsPage::scrollToSetting(const QString& settingId) {
    showSection(0);
    if (m_rows.contains(settingId)) {
        ensureWidgetVisible(m_rows[settingId]);
    }
}
