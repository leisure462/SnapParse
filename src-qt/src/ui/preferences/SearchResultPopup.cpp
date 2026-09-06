#include "SearchResultPopup.h"
#include <QVBoxLayout>

SearchResultPopup::SearchResultPopup(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_list = new QListWidget(this);
    m_list->setStyleSheet("QListWidget { background: #ffffff; border: 1px solid #e5e5ea; border-radius: 6px; }");
    layout->addWidget(m_list);

    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        QString tabId = item->data(Qt::UserRole).toString();
        QString sectionId = item->data(Qt::UserRole + 1).toString();
        QString settingId = item->data(Qt::UserRole + 2).toString();
        emit itemSelected(tabId, sectionId, settingId);
        hide();
    });

    initIndex();
    setFixedSize(260, 220);
}

void SearchResultPopup::initIndex() {
    m_allSearchItems = {
        {"record", "capture", "capture.text", "纯文本", "关闭后，复制的纯文本不会进入历史"},
        {"record", "capture", "capture.html", "HTML 内容", "关闭后，带 HTML 格式的网页富文本不会进入历史"},
        {"record", "capture", "capture.rtf", "RTF 内容", "关闭后，带 RTF 格式的文档富文本不会进入历史"},
        {"record", "capture", "capture.image", "图片", "关闭后，剪贴板中的图片不会进入历史"},
        {"record", "capture", "capture.files", "文件和文件夹", "关闭后，复制的文件和文件夹不会进入历史"},
        {"record", "capture", "capture.maxTextMb", "文本最大收录大小", "文本、HTML 或 RTF 超过此大小时不会进入历史"},
        {"record", "capture", "capture.maxImageMb", "图片最大收录大小", "图片超过此大小时不会进入历史"},
        {"record", "capture", "capture.order", "采集顺序", "剪贴板同时包含多种格式时，按这个顺序选择"},
        {"record", "source", "source.excludedApps", "忽略应用", "从这些应用复制的内容不会进入历史"},
        {"record", "sensitive", "sensitive.collectSecrets", "收录敏感内容", "检测到高置信度密钥、Token 等内容时是否保存"},
        {"record", "sensitive", "sensitive.redactSecrets", "脱敏显示", "对已收录的敏感内容保留头尾字符打码"},
        {"organize", "history", "history.retention", "保留周期", "超过该天数的普通记录会自动清理"},
        {"organize", "history", "history.maxCount", "最大保留条数", "超过上限后自动清理较旧的普通记录"},
        {"organize", "history", "history.cleanupIntervalHours", "自动清理周期", "每隔指定小时检查并清理一次历史记录"},
        {"organize", "organizing", "organizing.autoFavorite", "有备注时自动收藏", "保存非空备注时自动把该记录加入收藏"},
        {"organize", "groups", "organizing.customGroups", "自定义分组", "调整自定义分组的顺序、显示状态、名称和图标"},
        {"organize", "search", "search.defaultFocus", "打开窗口时聚焦搜索", "打开剪贴板窗口时自动选中搜索框"},
        {"organize", "search", "search.clearOnHide", "下次打开时清空搜索", "下次打开剪贴板窗口时清空搜索关键词"},
        {"organize", "search", "search.sort", "默认排序方式", "设置未搜索时历史列表的默认排序方式"},
        {"reuse", "paste", "paste.autoPaste", "鼠标左键", "选择鼠标左键点击历史记录时执行的动作"},
        {"reuse", "paste", "paste.middleClick", "鼠标中键", "选择鼠标中键点击历史记录时执行的动作"},
        {"reuse", "paste", "paste.plainDefault", "默认纯文本粘贴", "粘贴文本记录时默认去除格式"},
        {"reuse", "paste", "paste.fileMode", "默认文件路径粘贴", "粘贴文件记录时默认只粘贴文件路径"},
        {"reuse", "copy", "copy.plainDefault", "默认复制为纯文本", "复制文本记录时默认去除格式"},
        {"reuse", "copy", "copy.hideWindow", "复制后隐藏窗口", "从历史复制记录后自动隐藏剪贴板窗口"},
        {"reuse", "copy", "copy.updateOnReuse", "复用时更新记录", "从历史复制或粘贴记录时刷新使用时间"},
        {"reuse", "copy", "copy.sound", "复制成功提示音", "剪贴板新内容成功进入历史时播放提示音"},
        {"reuse", "actions", "actions.visible", "显示的快捷动作", "选择鼠标悬停在记录上时显示的操作按钮"},
        {"reuse", "actions", "actions.deleteConfirm", "普通条目删除前确认", "删除普通历史记录前先弹出确认"},
        {"reuse", "actions", "actions.deleteFavoriteItems", "允许删除收藏条目", "是否允许删除收藏条目"},
        {"reuse", "actions", "actions.deleteFavoriteConfirm", "收藏条目删除前确认", "删除已收藏记录前先弹出确认"},
        {"workflow", "window", "window.position", "打开位置", "选择每次打开剪贴板窗口时的位置"},
        {"workflow", "window", "window.scrollToTopOnOpen", "打开时回到顶部", "每次打开剪贴板窗口时自动回到顶部"},
        {"workflow", "preview", "preview.hover", "悬停预览", "鼠标悬停在记录上时显示完整内容预览"},
        {"workflow", "preview", "preview.delay", "悬停延迟", "设置鼠标悬停多久后显示预览"},
        {"workflow", "preview", "preview.space", "空格键预览", "按住空格键时预览当前选中的记录"},
        {"workflow", "appearance", "appearance.theme", "主题", "选择界面使用浅色、深色或跟随系统外观"},
        {"workflow", "appearance", "appearance.fontFamily", "界面字体", "自定义软件界面显示的字体，检测所有本机已安装字体"},
        {"workflow", "appearance", "appearance.language", "语言", "切换 SnapParse 的界面语言"},
        {"workflow", "appearance", "appearance.textMaxLines", "文本内容最大显示行数", "限制文本记录在列表卡片中显示的行数"},
        {"workflow", "appearance", "appearance.imageMaxHeight", "图片最大显示高度", "限制图片记录在列表卡片中的缩略图高度"},
        {"workflow", "appearance", "appearance.fileMaxCount", "文件最多显示数量", "限制文件记录在列表卡片中显示的文件数量"},
        {"workflow", "control", "control.autoStart", "开机启动", "登录系统后自动启动 SnapParse"},
        {"workflow", "control", "window.lightweightMode", "轻量模式", "隐藏剪贴板窗口后进入休眠减少内存占用"},
        {"workflow", "control", "control.trayIcon", "系统托盘图标", "在系统托盘显示 SnapParse 图标"},
        {"workflow", "control", "control.dockIcon", "任务栏图标", "在任务栏显示 SnapParse 图标"},
        {"shortcuts", "globalShortcuts", "shortcuts.openClipboard", "打开剪贴板窗口", "打开或隐藏剪贴板窗口"},
        {"shortcuts", "globalShortcuts", "shortcuts.openPreference", "打开偏好设置窗口", "打开或隐藏偏好设置窗口"},
        {"shortcuts", "globalShortcuts", "shortcuts.winV", "接管 Win+V", "按 Win+V 唤起 SnapParse 替代系统剪贴板"},
        {"data", "localData", "localData.dataDirectory", "数据目录", "打开历史数据库、设置文件和资源缓存所在目录"},
        {"data", "localData", "localData.cleanCache", "清理缓存", "移除不再被历史记录引用的图片缓存"},
        {"data", "backup", "backup.exportHistory", "导出备份", "导出 .snapparsebak 备份包"},
        {"data", "backup", "backup.importHistory", "导入备份", "导入 .snapparsebak 备份包"},
        {"data", "diagnostics", "diagnostics.resetPreferences", "重置所有偏好", "恢复所有偏好默认值，保留历史记录"},
        {"about", "about", "about.checkUpdates", "检查更新", "检查 SnapParse 是否有可用的新版本"},
        {"about", "about", "about.github", "GitHub", "打开开源仓库"}
    };
}

void SearchResultPopup::search(const QString& query) {
    m_list->clear();
    QString q = query.trimmed();
    if (q.isEmpty()) {
        hide();
        return;
    }

    for (const auto& item : m_allSearchItems) {
        if (item.title.contains(q, Qt::CaseInsensitive) || 
            item.description.contains(q, Qt::CaseInsensitive) ||
            item.settingId.contains(q, Qt::CaseInsensitive)) {
            
            QListWidgetItem* listItem = new QListWidgetItem(m_list);
            listItem->setText(item.title);
            listItem->setToolTip(item.description);
            listItem->setData(Qt::UserRole, item.tabId);
            listItem->setData(Qt::UserRole + 1, item.sectionId);
            listItem->setData(Qt::UserRole + 2, item.settingId);
        }
    }

    if (m_list->count() > 0) {
        show();
    } else {
        hide();
    }
}
