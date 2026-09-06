#include "RecordPage.h"
#include "AppConfig.h"
#include "DatabaseManager.h"
#include "SortableTreeDialog.h"
#include "ThemeManager.h"
#include "SystemProcessManager.h"
#include "Logger.h"
#include <QFileInfo>
#include <algorithm>

RecordPage::RecordPage(QWidget* parent) : QScrollArea(parent) {
    Logger::info("RecordPage constructor start");
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setupUi();
    loadData();
    showSection(0);
    Logger::info("RecordPage constructor end");
}

void RecordPage::setupUi() {
    QWidget* container = new QWidget(this);
    m_contentLayout = new QVBoxLayout(container);
    m_contentLayout->setContentsMargins(20, 16, 20, 24);
    m_contentLayout->setSpacing(18);

    auto cfg = AppConfig::instance();

    // ==========================================
    // Section 1: Capture Group
    // ==========================================
    m_captureGroup = new FluentSettingCardGroup("内容类型", container);

    auto* cardText = new FluentSwitchSettingCard(FluentIconType::FileText, "纯文本", "关闭后，复制的纯文本不会进入历史。", this);
    cardText->setChecked(cfg->capture.text);
    connect(cardText, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->capture.text = chk;
        AppConfig::instance()->save();
    });
    m_rows["capture.text"] = cardText;
    m_captureGroup->addSettingCard(cardText);

    auto* cardHtml = new FluentSwitchSettingCard(FluentIconType::Globe, "HTML 内容", "关闭后，带 HTML 格式的网页富文本不会进入历史。", this);
    cardHtml->setChecked(cfg->capture.html);
    connect(cardHtml, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->capture.html = chk;
        AppConfig::instance()->save();
    });
    m_rows["capture.html"] = cardHtml;
    m_captureGroup->addSettingCard(cardHtml);

    auto* cardRtf = new FluentSwitchSettingCard(FluentIconType::FileText, "RTF 内容", "关闭后，带 RTF 格式的文档富文本不会进入历史。", this);
    cardRtf->setChecked(cfg->capture.rtf);
    connect(cardRtf, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->capture.rtf = chk;
        AppConfig::instance()->save();
    });
    m_rows["capture.rtf"] = cardRtf;
    m_captureGroup->addSettingCard(cardRtf);

    auto* cardImage = new FluentSwitchSettingCard(FluentIconType::Image, "图片", "关闭后，剪贴板中的图片不会进入历史。", this);
    cardImage->setChecked(cfg->capture.image);
    connect(cardImage, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->capture.image = chk;
        AppConfig::instance()->save();
    });
    m_rows["capture.image"] = cardImage;
    m_captureGroup->addSettingCard(cardImage);

    auto* cardFiles = new FluentSwitchSettingCard(FluentIconType::Folder, "文件和文件夹", "关闭后，复制的文件和文件夹不会进入历史。", this);
    cardFiles->setChecked(cfg->capture.files);
    connect(cardFiles, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->capture.files = chk;
        AppConfig::instance()->save();
    });
    m_rows["capture.files"] = cardFiles;
    m_captureGroup->addSettingCard(cardFiles);

    auto* cardTextMb = new FluentRangeSettingCard(FluentIconType::FileText, "文本最大体积", "超过该体积的文本不会被记录，0 表示不限制。", this);
    cardTextMb->setRange(0, 100);
    cardTextMb->setSuffix(" MB");
    cardTextMb->setValue(cfg->capture.maxTextMb);
    connect(cardTextMb, &FluentRangeSettingCard::valueChanged, this, [](int v) {
        AppConfig::instance()->capture.maxTextMb = v;
        AppConfig::instance()->save();
    });
    m_rows["capture.maxTextMb"] = cardTextMb;
    m_captureGroup->addSettingCard(cardTextMb);

    auto* cardImageMb = new FluentRangeSettingCard(FluentIconType::Image, "图片最大体积", "超过该体积的图片不会被记录，0 表示不限制。", this);
    cardImageMb->setRange(0, 100);
    cardImageMb->setSuffix(" MB");
    cardImageMb->setValue(cfg->capture.maxImageMb);
    connect(cardImageMb, &FluentRangeSettingCard::valueChanged, this, [](int v) {
        AppConfig::instance()->capture.maxImageMb = v;
        AppConfig::instance()->save();
    });
    m_rows["capture.maxImageMb"] = cardImageMb;
    m_captureGroup->addSettingCard(cardImageMb);

    auto* cardOrder = new FluentPushSettingCard(FluentIconType::Workflow, "调整优先级", "内容采集优先级", "当一次复制包含多种格式时，优先采集的内容类型。", this);
    connect(cardOrder, &FluentPushSettingCard::clicked, this, [this]() {
        SortableTreeDialog dlg("调整内容采集优先级", false, this);
        auto cfg = AppConfig::instance();
        QMap<QString, QString> itemMap = {
            {"files", "文件和文件夹"},
            {"image", "图片"},
            {"html", "HTML 内容"},
            {"rtf", "RTF 内容"},
            {"text", "纯文本"}
        };
        QList<QPair<QString, QString>> items;
        for (const QString& k : cfg->capture.order) {
            if (itemMap.contains(k)) {
                items.append({k, itemMap[k]});
                itemMap.remove(k);
            }
        }
        for (auto it = itemMap.begin(); it != itemMap.end(); ++it) {
            items.append({it.key(), it.value()});
        }

        dlg.setItems(items);
        if (dlg.exec() == QDialog::Accepted) {
            AppConfig::instance()->capture.order = dlg.orderedKeys();
            AppConfig::instance()->save();
        }
    });
    m_rows["capture.order"] = cardOrder;
    m_captureGroup->addSettingCard(cardOrder);

    m_contentLayout->addWidget(m_captureGroup);

    // ==========================================
    // Section 2: Source App Filtering Group (Modern Fluent Design)
    // ==========================================
    QWidget* sourceContainer = new QWidget(container);
    QVBoxLayout* sourceLayout = new QVBoxLayout(sourceContainer);
    sourceLayout->setContentsMargins(0, 8, 0, 8);
    sourceLayout->setSpacing(10);

    QLabel* sourceTitle = new QLabel("应用过滤", sourceContainer);
    sourceTitle->setStyleSheet("font-size: 14px; font-weight: 600;");
    sourceLayout->addWidget(sourceTitle);

    m_appFilter = new FluentAppFilterWidget(sourceContainer);
    connect(m_appFilter, &FluentAppFilterWidget::excludedAppsChanged, this, [](const QStringList& excluded) {
        AppConfig::instance()->filters.excludedAppIds = excluded;
        AppConfig::instance()->save();
    });
    connect(m_appFilter, &FluentAppFilterWidget::refreshRequested, this, &RecordPage::loadData);
    m_rows["source.excludedAppIds"] = m_appFilter;
    sourceLayout->addWidget(m_appFilter);

    m_sourceGroup = sourceContainer;
    m_contentLayout->addWidget(sourceContainer);

    // ==========================================
    // Section 3: Privacy & Sensitive Group
    // ==========================================
    m_sensitiveGroup = new FluentSettingCardGroup("隐私保护", container);

    auto* cardCollect = new FluentSwitchSettingCard(FluentIconType::Shield, "采集密码和敏感数据", "关闭后，检测到密码或 Token 等敏感信息时不会记录到历史。", this);
    cardCollect->setChecked(cfg->sensitive.collectSecrets);
    connect(cardCollect, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->sensitive.collectSecrets = chk;
        AppConfig::instance()->save();
    });
    m_rows["sensitive.collectSecrets"] = cardCollect;
    m_sensitiveGroup->addSettingCard(cardCollect);

    auto* cardRedact = new FluentSwitchSettingCard(FluentIconType::Shield, "敏感数据脱敏展示", "开启后，列表中涉及敏感信息的条目会使用掩码替代。", this);
    cardRedact->setChecked(cfg->sensitive.redactSecrets);
    connect(cardRedact, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->sensitive.redactSecrets = chk;
        AppConfig::instance()->save();
    });
    m_rows["sensitive.redactSecrets"] = cardRedact;
    m_sensitiveGroup->addSettingCard(cardRedact);

    m_contentLayout->addWidget(m_sensitiveGroup);
    m_contentLayout->addStretch();

    setWidget(container);
}

void RecordPage::loadData() {
    auto dbApps = DatabaseManager::instance()->listApps();
    auto runningApps = SystemProcessManager::listRunningApps();
    QStringList excluded = AppConfig::instance()->filters.excludedAppIds;

    // Merge running apps, historical DB apps, and explicitly excluded apps
    QMap<QString, ClipboardApp> merged;
    for (const auto& a : runningApps) {
        merged[a.id.toLower()] = a;
    }
    for (const auto& a : dbApps) {
        QString idLower = a.id.toLower();
        if (!merged.contains(idLower)) {
            merged[idLower] = a;
        } else {
            if (!a.name.isEmpty()) merged[idLower].name = a.name;
            if (!a.iconFile.isEmpty()) merged[idLower].iconFile = a.iconFile;
        }
    }
    for (const QString& exId : excluded) {
        QString exLower = exId.toLower();
        if (!merged.contains(exLower)) {
            ClipboardApp app;
            app.id = exLower;
            app.name = QFileInfo(exLower).completeBaseName();
            app.platform = "windows";
            merged[exLower] = app;
        }
    }

    QList<ClipboardApp> appList = merged.values();
    std::sort(appList.begin(), appList.end(), [](const ClipboardApp& a, const ClipboardApp& b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });

    m_appFilter->setAvailableApps(appList);
    m_appFilter->setExcludedAppIds(excluded);
}

void RecordPage::showSection(int index) {
    if (index == 0) {
        m_captureGroup->show();
        m_sourceGroup->hide();
        m_sensitiveGroup->hide();
    } else if (index == 1) {
        m_captureGroup->hide();
        m_sourceGroup->show();
        m_sensitiveGroup->hide();
        loadData(); // Re-scan running processes whenever user opens the filter tab
    } else if (index == 2) {
        m_captureGroup->hide();
        m_sourceGroup->hide();
        m_sensitiveGroup->show();
    } else {
        m_captureGroup->show();
        m_sourceGroup->show();
        m_sensitiveGroup->show();
    }
}

void RecordPage::scrollToSetting(const QString& settingId) {
    if (settingId.startsWith("capture.")) {
        showSection(0);
    } else if (settingId.startsWith("source.")) {
        showSection(1);
    } else if (settingId.startsWith("sensitive.")) {
        showSection(2);
    }
    if (m_rows.contains(settingId)) {
        ensureWidgetVisible(m_rows[settingId]);
    }
}
