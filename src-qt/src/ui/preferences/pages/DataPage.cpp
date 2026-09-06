#include "DataPage.h"
#include "AppConfig.h"
#include "AppPaths.h"
#include "DatabaseManager.h"
#include "BackupService.h"
#include "ThemeManager.h"
#include "ClipboardCardDelegate.h"
#include "EventBus.h"
#include "Logger.h"

#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QDir>
#include <QFile>

static bool copyDirectoryRecursively(const QString& srcPath, const QString& dstPath) {
    QDir srcDir(srcPath);
    if (!srcDir.exists()) return true;
    QDir dstDir(dstPath);
    if (!dstDir.exists()) {
        if (!dstDir.mkpath(dstPath)) return false;
    }

    const auto fileInfos = srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const auto& fileInfo : fileInfos) {
        QString srcItem = fileInfo.filePath();
        QString dstItem = dstPath + "/" + fileInfo.fileName();
        if (fileInfo.isDir()) {
            if (!copyDirectoryRecursively(srcItem, dstItem)) return false;
        } else {
            if (QFile::exists(dstItem)) {
                QFile::remove(dstItem);
            }
            if (!QFile::copy(srcItem, dstItem)) {
                return false;
            }
        }
    }
    return true;
}

DataPage::DataPage(QWidget* parent) : QScrollArea(parent) {
    Logger::info("DataPage constructor start");
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setupUi();
    showSection(0);
    Logger::info("DataPage constructor end");
}

void DataPage::setupUi() {
    QWidget* container = new QWidget(this);
    m_contentLayout = new QVBoxLayout(container);
    m_contentLayout->setContentsMargins(20, 16, 20, 24);
    m_contentLayout->setSpacing(18);

    auto cfg = AppConfig::instance();

    // Section 1: Storage Location Group
    m_storageGroup = new FluentSettingCardGroup("存储位置", container);

    auto* cardPath = new FluentPushSettingCard(FluentIconType::Folder, "更改路径", "本地数据存储路径", AppPaths::currentDataDir(), this);
    connect(cardPath, &FluentPushSettingCard::clicked, this, [this, cardPath]() {
        QString currentDir = AppPaths::currentDataDir();
        QString newDir = QFileDialog::getExistingDirectory(this, "选择数据存储目录", currentDir);
        if (newDir.isEmpty()) return;

        QDir d1(newDir);
        QDir d2(currentDir);
        if (d1.canonicalPath() == d2.canonicalPath()) return;

        // Perform live hot migration
        DatabaseManager::instance()->close();

        bool ok = copyDirectoryRecursively(currentDir, newDir);
        if (!ok) {
            // Clean up partially copied files in destination
            QDir destDir(newDir);
            if (destDir.exists()) {
                destDir.removeRecursively();
            }
            // Restore database in previous directory if copy had issues
            DatabaseManager::instance()->init(AppPaths::databasePathFor(currentDir));
            QMessageBox::warning(this, "迁移失败", "数据迁移过程中发生错误，已保持原有存储路径并清理了目标目录中的残留文件。请检查目标文件夹写入权限。");
            return;
        }

        // Update active data directory
        AppPaths::setCurrentDataDir(newDir);

        // Reconnect live database immediately
        DatabaseManager::instance()->init(AppPaths::databasePathFor(newDir));

        // Clear in-memory caches and notify UI
        ClipboardCardDelegate::clearThumbnailCache();
        EventBus::instance()->clipboardUpdated();
        EventBus::instance()->settingsChanged("storage");

        cardPath->setContent(newDir);
        QMessageBox::information(this, "迁移成功", "剪贴板历史数据与图片缓存已完整迁移至新位置，并已即时生效！无需重启软件。");
    });
    m_rows["localData.path"] = cardPath;
    m_storageGroup->addSettingCard(cardPath);

    auto* cardOpen = new FluentPushSettingCard(FluentIconType::Folder, "打开目录", "在文件资源管理器中浏览", "快速打开数据库及本地缓存所在文件夹。", this);
    connect(cardOpen, &FluentPushSettingCard::clicked, this, []() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::currentDataDir()));
    });
    m_rows["localData.openFolder"] = cardOpen;
    m_storageGroup->addSettingCard(cardOpen);

    m_contentLayout->addWidget(m_storageGroup);

    // Section 2: Backup Group
    m_backupGroup = new FluentSettingCardGroup("备份迁移", container);

    auto* cardExport = new FluentPushSettingCard(FluentIconType::Data, "导出备份", "备份数据库", "将全部剪贴板记录导出为压缩包备份文件（.snapparse）。", this);
    connect(cardExport, &FluentPushSettingCard::clicked, this, [this]() {
        QString file = QFileDialog::getSaveFileName(this, "导出备份", "snapparse_backup.zip", "SnapParse Backup (*.zip *.snapparse)");
        if (!file.isEmpty()) {
            if (BackupService::exportBackup(file, true)) {
                QMessageBox::information(this, "成功", "备份导出成功！");
            } else {
                QMessageBox::warning(this, "失败", "备份导出失败，请检查写入权限。");
            }
        }
    });
    m_rows["backup.export"] = cardExport;
    m_backupGroup->addSettingCard(cardExport);

    auto* cardImport = new FluentPushSettingCard(FluentIconType::Data, "导入备份", "恢复数据库", "从备份文件中恢复剪贴板历史记录。", this);
    connect(cardImport, &FluentPushSettingCard::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "导入备份", "", "SnapParse Backup (*.zip *.snapparse)");
        if (!file.isEmpty()) {
            if (BackupService::importBackup(file)) {
                QMessageBox::information(this, "成功", "备份恢复成功！请重启软件以加载新数据。");
            } else {
                QMessageBox::warning(this, "失败", "备份文件无效或损坏。");
            }
        }
    });
    m_rows["backup.import"] = cardImport;
    m_backupGroup->addSettingCard(cardImport);

    m_contentLayout->addWidget(m_backupGroup);

    // Section 3: Maintenance Group
    m_diagGroup = new FluentSettingCardGroup("诊断恢复", container);

    auto* cardCache = new FluentPushSettingCard(FluentIconType::Trash, "立即清理", "清理孤立图片缓存", "扫描并清理数据库中已无引用的本地图片缓存文件。", this);
    connect(cardCache, &FluentPushSettingCard::clicked, this, [this]() {
        qint64 freed = DatabaseManager::instance()->cleanupUnreferencedCache();
        QMessageBox::information(this, "清理完成", QString("已释放 %1 MB 磁盘空间").arg(freed / (1024.0 * 1024.0), 0, 'f', 2));
    });
    m_rows["diagnostics.cleanCache"] = cardCache;
    m_diagGroup->addSettingCard(cardCache);

    auto* cardVacuum = new FluentPushSettingCard(FluentIconType::Data, "碎片整理", "整理数据库碎片", "对 SQLite 数据库执行 VACUUM，压缩存储并提高检索性能。", this);
    connect(cardVacuum, &FluentPushSettingCard::clicked, this, [this]() {
        DatabaseManager::instance()->vacuum();
        QMessageBox::information(this, "完成", "数据库碎片整理完成！");
    });
    m_rows["diagnostics.vacuum"] = cardVacuum;
    m_diagGroup->addSettingCard(cardVacuum);

    m_contentLayout->addWidget(m_diagGroup);

    // Section 4: Updates Group
    m_updateGroup = new FluentSettingCardGroup("更新", container);

    auto* cardCheck = new FluentSwitchSettingCard(FluentIconType::About, "启动时自动检查更新", "发现新版本时在托盘或主窗口给出更新提醒。", this);
    cardCheck->setChecked(cfg->update.autoCheck);
    connect(cardCheck, &FluentSwitchSettingCard::checkedChanged, this, [](bool chk) {
        AppConfig::instance()->update.autoCheck = chk;
        AppConfig::instance()->save();
    });
    m_rows["updates.autoCheck"] = cardCheck;
    m_updateGroup->addSettingCard(cardCheck);

    m_contentLayout->addWidget(m_updateGroup);
    m_contentLayout->addStretch();

    setWidget(container);
}

void DataPage::showSection(int index) {
    if (index == 0) {
        m_storageGroup->show();
        m_backupGroup->hide();
        m_diagGroup->hide();
        m_updateGroup->hide();
    } else if (index == 1) {
        m_storageGroup->hide();
        m_backupGroup->show();
        m_diagGroup->hide();
        m_updateGroup->hide();
    } else if (index == 2) {
        m_storageGroup->hide();
        m_backupGroup->hide();
        m_diagGroup->show();
        m_updateGroup->hide();
    } else if (index == 3) {
        m_storageGroup->hide();
        m_backupGroup->hide();
        m_diagGroup->hide();
        m_updateGroup->show();
    } else {
        m_storageGroup->show();
        m_backupGroup->show();
        m_diagGroup->show();
        m_updateGroup->show();
    }
}

void DataPage::scrollToSetting(const QString& settingId) {
    if (settingId.startsWith("localData.")) {
        showSection(0);
    } else if (settingId.startsWith("backup.")) {
        showSection(1);
    } else if (settingId.startsWith("diagnostics.")) {
        showSection(2);
    } else if (settingId.startsWith("updates.")) {
        showSection(3);
    }
    if (m_rows.contains(settingId)) {
        ensureWidgetVisible(m_rows[settingId]);
    }
}
