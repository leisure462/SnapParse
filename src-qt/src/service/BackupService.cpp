#include "BackupService.h"
#include "DatabaseManager.h"
#include "AppPaths.h"
#include "Logger.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSqlQuery>

bool BackupService::exportBackup(const QString& targetFilePath, bool encrypted, const QString& password) {
    Q_UNUSED(encrypted)
    Q_UNUSED(password)

    QJsonObject root;
    root["version"] = "1.0.0";
    root["app"] = "SnapParse";
    root["exported_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    auto db = DatabaseManager::instance();

    // Groups
    QJsonArray groupsArr;
    for (const auto& g : db->listGroups(true)) {
        QJsonObject o;
        o["id"] = g.id;
        o["name"] = g.name;
        o["icon"] = g.icon;
        o["is_hidden"] = g.isHidden;
        o["sort_order"] = g.sortOrder;
        o["created_at"] = g.createdAt;
        o["updated_at"] = g.updatedAt;
        groupsArr.append(o);
    }
    root["groups"] = groupsArr;

    // Apps
    QJsonArray appsArr;
    for (const auto& a : db->listApps()) {
        QJsonObject o;
        o["id"] = a.id;
        o["name"] = a.name;
        o["icon_file"] = a.iconFile;
        o["platform"] = a.platform;
        o["created_at"] = a.createdAt;
        o["updated_at"] = a.updatedAt;
        appsArr.append(o);
    }
    root["apps"] = appsArr;

    // Items
    QJsonArray itemsArr;
    for (const auto& item : db->queryItems("all", "all", "", "", "updatedAtDesc", 100000, 0)) {
        QJsonObject o;
        o["id"] = item.id;
        o["kind"] = item.kind;
        o["sub_kind"] = item.subKind;
        o["group_id"] = item.groupId;
        o["source_app_id"] = item.sourceAppId;
        o["content"] = item.content;
        o["content_hash"] = item.contentHash;
        o["search_text"] = item.searchText;
        o["summary"] = item.summary;
        o["file_types"] = item.fileTypes;
        o["size"] = item.size;
        o["width"] = item.width;
        o["height"] = item.height;
        o["use_count"] = item.useCount;
        o["is_favorite"] = item.isFavorite;
        o["is_pinned"] = item.isPinned;
        o["is_sensitive"] = item.isSensitive;
        o["platform"] = item.platform;
        o["note"] = item.note;
        o["created_at"] = item.createdAt;
        o["updated_at"] = item.updatedAt;
        itemsArr.append(o);
    }
    root["items"] = itemsArr;

    QFile file(targetFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        Logger::error("Failed to open backup target file for writing: " + targetFilePath);
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Compact));
    return true;
}

bool BackupService::importBackup(const QString& sourceFilePath, const QString& strategy, const QString& password) {
    Q_UNUSED(password)

    QFile file(sourceFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::error("Failed to open backup file for reading: " + sourceFilePath);
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        Logger::error("Invalid backup file JSON");
        return false;
    }

    QJsonObject root = doc.object();
    auto db = DatabaseManager::instance();

    if (strategy == "overwrite") {
        // Clear existing database items and groups
        QSqlQuery q;
        q.exec("DELETE FROM clipboard_items;");
        q.exec("DELETE FROM clipboard_groups;");
        q.exec("DELETE FROM clipboard_apps;");
    }

    // Import groups
    QJsonArray groupsArr = root["groups"].toArray();
    for (const auto& val : groupsArr) {
        QJsonObject o = val.toObject();
        ClipboardGroup g;
        g.id = o["id"].toString();
        g.name = o["name"].toString();
        g.icon = o["icon"].toString();
        g.isHidden = o["is_hidden"].toBool();
        g.sortOrder = o["sort_order"].toInt();
        g.createdAt = o["created_at"].toString();
        g.updatedAt = o["updated_at"].toString();
        db->createGroup(g);
    }

    // Import apps
    QJsonArray appsArr = root["apps"].toArray();
    for (const auto& val : appsArr) {
        QJsonObject o = val.toObject();
        ClipboardApp a;
        a.id = o["id"].toString();
        a.name = o["name"].toString();
        a.iconFile = o["icon_file"].toString();
        a.platform = o["platform"].toString();
        a.createdAt = o["created_at"].toString();
        a.updatedAt = o["updated_at"].toString();
        db->createOrUpdateApp(a);
    }

    // Import items
    QJsonArray itemsArr = root["items"].toArray();
    for (const auto& val : itemsArr) {
        QJsonObject o = val.toObject();
        ClipboardItem item;
        item.id = o["id"].toString();
        item.kind = o["kind"].toString();
        item.subKind = o["sub_kind"].toString();
        item.groupId = o["group_id"].toString();
        item.sourceAppId = o["source_app_id"].toString();
        item.content = o["content"].toString();
        item.contentHash = o["content_hash"].toString();
        item.searchText = o["search_text"].toString();
        item.summary = o["summary"].toString();
        item.fileTypes = o["file_types"].toString();
        item.size = o["size"].toInteger();
        item.width = o["width"].toInt();
        item.height = o["height"].toInt();
        item.useCount = o["use_count"].toInt();
        item.isFavorite = o["is_favorite"].toBool();
        item.isPinned = o["is_pinned"].toBool();
        item.isSensitive = o["is_sensitive"].toBool();
        item.platform = o["platform"].toString();
        item.note = o["note"].toString();
        item.createdAt = o["created_at"].toString();
        item.updatedAt = o["updated_at"].toString();

        db->insertItem(item);
    }

    return true;
}
