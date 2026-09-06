#include "DatabaseManager.h"
#include "AppPaths.h"
#include "Logger.h"
#include "EventBus.h"
#include <QUuid>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>

DatabaseManager* DatabaseManager::instance() {
    static DatabaseManager s_instance;
    return &s_instance;
}

DatabaseManager::DatabaseManager(QObject* parent) : QObject(parent) {
    m_connectionName = "ecopaste_main_db";
}

DatabaseManager::~DatabaseManager() {
    close();
}

bool DatabaseManager::init(const QString& dbPath) {
    QMutexLocker locker(&m_mutex);
    QString path = dbPath.isEmpty() ? AppPaths::databasePath() : dbPath;
    Logger::info("Initializing database at " + path);

    if (QSqlDatabase::contains(m_connectionName)) {
        m_db = QSqlDatabase::database(m_connectionName);
    } else {
        m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    }

    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        Logger::error("Failed to open database: " + m_db.lastError().text());
        return false;
    }

    // Enable WAL mode, cache limit, and foreign keys for high performance
    QSqlQuery q(m_db);
    q.exec("PRAGMA journal_mode = WAL;");
    q.exec("PRAGMA foreign_keys = ON;");
    q.exec("PRAGMA synchronous = NORMAL;");
    q.exec("PRAGMA cache_size = -2000;");
    q.exec("PRAGMA temp_store = MEMORY;");

    return createTables();
}

void DatabaseManager::close() {
    QMutexLocker locker(&m_mutex);
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::reconnect(const QString& newDbPath) {
    close();
    return init(newDbPath);
}

bool DatabaseManager::createTables() {
    QSqlQuery q(m_db);

    // Groups
    bool ok = q.exec(
        "CREATE TABLE IF NOT EXISTS clipboard_groups ("
        "  id          TEXT    PRIMARY KEY,"
        "  name        TEXT    NOT NULL,"
        "  icon        TEXT    NOT NULL,"
        "  is_hidden   INTEGER NOT NULL DEFAULT 0,"
        "  sort_order  INTEGER NOT NULL DEFAULT 0,"
        "  created_at  TEXT    NOT NULL,"
        "  updated_at  TEXT    NOT NULL"
        ");"
    );
    if (!ok) {
        Logger::error("Failed to create clipboard_groups: " + q.lastError().text());
        return false;
    }

    // Apps
    ok = q.exec(
        "CREATE TABLE IF NOT EXISTS clipboard_apps ("
        "  id          TEXT    PRIMARY KEY,"
        "  name        TEXT    NOT NULL,"
        "  icon_file   TEXT,"
        "  platform    TEXT    NOT NULL,"
        "  created_at  TEXT    NOT NULL,"
        "  updated_at  TEXT    NOT NULL"
        ");"
    );
    if (!ok) {
        Logger::error("Failed to create clipboard_apps: " + q.lastError().text());
        return false;
    }

    // Items
    ok = q.exec(
        "CREATE TABLE IF NOT EXISTS clipboard_items ("
        "  id            TEXT    PRIMARY KEY,"
        "  kind          TEXT    NOT NULL,"
        "  sub_kind      TEXT,"
        "  group_id      TEXT    REFERENCES clipboard_groups(id) ON DELETE SET NULL,"
        "  source_app_id TEXT    REFERENCES clipboard_apps(id)   ON DELETE SET NULL,"
        "  content       TEXT    NOT NULL,"
        "  content_hash  TEXT    NOT NULL,"
        "  search_text   TEXT,"
        "  summary       TEXT,"
        "  file_types    TEXT,"
        "  size          INTEGER,"
        "  width         INTEGER,"
        "  height        INTEGER,"
        "  use_count     INTEGER NOT NULL DEFAULT 1,"
        "  is_favorite   INTEGER NOT NULL DEFAULT 0,"
        "  is_pinned     INTEGER NOT NULL DEFAULT 0,"
        "  is_sensitive  INTEGER NOT NULL DEFAULT 0,"
        "  platform      TEXT    NOT NULL,"
        "  note          TEXT,"
        "  created_at    TEXT    NOT NULL,"
        "  updated_at    TEXT    NOT NULL"
        ");"
    );
    if (!ok) {
        Logger::error("Failed to create clipboard_items: " + q.lastError().text());
        return false;
    }

    q.exec("CREATE INDEX IF NOT EXISTS idx_clipboard_items_content_hash ON clipboard_items (content_hash);");
    q.exec("CREATE INDEX IF NOT EXISTS idx_clipboard_items_source_app_id ON clipboard_items (source_app_id);");
    q.exec("CREATE INDEX IF NOT EXISTS idx_clipboard_items_updated_at ON clipboard_items (updated_at);");
    q.exec("CREATE INDEX IF NOT EXISTS idx_clipboard_items_kind ON clipboard_items (kind);");
    q.exec("CREATE INDEX IF NOT EXISTS idx_clipboard_items_fav ON clipboard_items (is_favorite);");
    q.exec("CREATE INDEX IF NOT EXISTS idx_clipboard_items_pin ON clipboard_items (is_pinned);");
    q.exec("CREATE INDEX IF NOT EXISTS idx_clipboard_items_comp ON clipboard_items (is_pinned, is_favorite, updated_at DESC);");

    // FTS5 Virtual Table & Triggers
    q.exec(
        "CREATE VIRTUAL TABLE IF NOT EXISTS clipboard_items_fts USING fts5("
        "  search_text,"
        "  note,"
        "  content='clipboard_items',"
        "  content_rowid='rowid'"
        ");"
    );

    q.exec(
        "CREATE TRIGGER IF NOT EXISTS clipboard_items_ai AFTER INSERT ON clipboard_items BEGIN "
        "  INSERT INTO clipboard_items_fts(rowid, search_text, note) VALUES (new.rowid, new.search_text, new.note); "
        "END;"
    );
    q.exec(
        "CREATE TRIGGER IF NOT EXISTS clipboard_items_ad AFTER DELETE ON clipboard_items BEGIN "
        "  INSERT INTO clipboard_items_fts(clipboard_items_fts, rowid, search_text, note) VALUES ('delete', old.rowid, old.search_text, old.note); "
        "END;"
    );
    q.exec(
        "CREATE TRIGGER IF NOT EXISTS clipboard_items_au AFTER UPDATE ON clipboard_items BEGIN "
        "  INSERT INTO clipboard_items_fts(clipboard_items_fts, rowid, search_text, note) VALUES ('delete', old.rowid, old.search_text, old.note); "
        "  INSERT INTO clipboard_items_fts(rowid, search_text, note) VALUES (new.rowid, new.search_text, new.note); "
        "END;"
    );

    return true;
}

ClipboardItem DatabaseManager::itemFromQuery(const QSqlQuery& q) {
    ClipboardItem item;
    item.id = q.value("id").toString();
    item.kind = q.value("kind").toString();
    item.subKind = q.value("sub_kind").toString();
    item.groupId = q.value("group_id").toString();
    item.sourceAppId = q.value("source_app_id").toString();
    item.content = q.value("content").toString();
    item.contentHash = q.value("content_hash").toString();
    item.searchText = q.value("search_text").toString();
    item.summary = q.value("summary").toString();
    item.fileTypes = q.value("file_types").toString();
    item.size = q.value("size").toLongLong();
    item.width = q.value("width").toInt();
    item.height = q.value("height").toInt();
    item.useCount = q.value("use_count").toInt();
    item.isFavorite = q.value("is_favorite").toInt() != 0;
    item.isPinned = q.value("is_pinned").toInt() != 0;
    item.isSensitive = q.value("is_sensitive").toInt() != 0;
    item.platform = q.value("platform").toString();
    item.note = q.value("note").toString();
    item.createdAt = q.value("created_at").toString();
    item.updatedAt = q.value("updated_at").toString();

    // Joined fields if present
    item.sourceAppName = q.value("app_name").toString();
    item.sourceAppIcon = q.value("app_icon").toString();

    return item;
}

bool DatabaseManager::insertItem(ClipboardItem& item) {
    QMutexLocker locker(&m_mutex);

    // Check if exists by contentHash
    QSqlQuery check(m_db);
    check.prepare("SELECT id, use_count FROM clipboard_items WHERE content_hash = :hash");
    check.bindValue(":hash", item.contentHash);
    if (check.exec() && check.next()) {
        // Reuse existing
        QString existingId = check.value("id").toString();
        int useCount = check.value("use_count").toInt() + 1;
        QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

        QSqlQuery update(m_db);
        update.prepare("UPDATE clipboard_items SET use_count = :uc, updated_at = :ua WHERE id = :id");
        update.bindValue(":uc", useCount);
        update.bindValue(":ua", now);
        update.bindValue(":id", existingId);
        update.exec();

        item.id = existingId;
        item.useCount = useCount;
        item.updatedAt = now;
        return true;
    }

    if (item.id.isEmpty()) {
        item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    if (item.createdAt.isEmpty()) item.createdAt = now;
    if (item.updatedAt.isEmpty()) item.updatedAt = now;

    QSqlQuery q(m_db);
    q.prepare(
        "INSERT INTO clipboard_items ("
        "  id, kind, sub_kind, group_id, source_app_id, content, content_hash, "
        "  search_text, summary, file_types, size, width, height, use_count, "
        "  is_favorite, is_pinned, is_sensitive, platform, note, created_at, updated_at"
        ") VALUES ("
        "  :id, :kind, :sub_kind, :group_id, :source_app_id, :content, :content_hash, "
        "  :search_text, :summary, :file_types, :size, :width, :height, :use_count, "
        "  :is_favorite, :is_pinned, :is_sensitive, :platform, :note, :created_at, :updated_at"
        ")"
    );

    q.bindValue(":id", item.id);
    q.bindValue(":kind", item.kind);
    q.bindValue(":sub_kind", item.subKind.isEmpty() ? QVariant(QVariant::String) : item.subKind);
    q.bindValue(":group_id", item.groupId.isEmpty() ? QVariant(QVariant::String) : item.groupId);
    q.bindValue(":source_app_id", item.sourceAppId.isEmpty() ? QVariant(QVariant::String) : item.sourceAppId);
    q.bindValue(":content", item.content);
    q.bindValue(":content_hash", item.contentHash);
    q.bindValue(":search_text", item.searchText);
    q.bindValue(":summary", item.summary);
    q.bindValue(":file_types", item.fileTypes);
    q.bindValue(":size", item.size);
    q.bindValue(":width", item.width);
    q.bindValue(":height", item.height);
    q.bindValue(":use_count", item.useCount);
    q.bindValue(":is_favorite", item.isFavorite ? 1 : 0);
    q.bindValue(":is_pinned", item.isPinned ? 1 : 0);
    q.bindValue(":is_sensitive", item.isSensitive ? 1 : 0);
    q.bindValue(":platform", item.platform);
    q.bindValue(":note", item.note.isEmpty() ? QVariant(QVariant::String) : item.note);
    q.bindValue(":created_at", item.createdAt);
    q.bindValue(":updated_at", item.updatedAt);

    if (!q.exec()) {
        Logger::error("Insert item failed: " + q.lastError().text());
        return false;
    }

    return true;
}

bool DatabaseManager::updateItem(const ClipboardItem& item) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare(
        "UPDATE clipboard_items SET "
        "  sub_kind = :sub_kind, group_id = :group_id, note = :note, "
        "  is_favorite = :is_favorite, is_pinned = :is_pinned "
        "WHERE id = :id"
    );
    q.bindValue(":sub_kind", item.subKind);
    q.bindValue(":group_id", item.groupId.isEmpty() ? QVariant(QVariant::String) : item.groupId);
    q.bindValue(":note", item.note);
    q.bindValue(":is_favorite", item.isFavorite ? 1 : 0);
    q.bindValue(":is_pinned", item.isPinned ? 1 : 0);
    q.bindValue(":id", item.id);
    return q.exec();
}

bool DatabaseManager::deleteItem(const QString& id) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM clipboard_items WHERE id = :id");
    q.bindValue(":id", id);
    return q.exec();
}

std::optional<ClipboardItem> DatabaseManager::getItem(const QString& id) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare(
        "SELECT i.*, a.name AS app_name, a.icon_file AS app_icon "
        "FROM clipboard_items i "
        "LEFT JOIN clipboard_apps a ON i.source_app_id = a.id "
        "WHERE i.id = :id"
    );
    q.bindValue(":id", id);
    if (q.exec() && q.next()) {
        return itemFromQuery(q);
    }
    return std::nullopt;
}

std::optional<ClipboardItem> DatabaseManager::findItemByHash(const QString& hash) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM clipboard_items WHERE content_hash = :hash");
    q.bindValue(":hash", hash);
    if (q.exec() && q.next()) {
        return itemFromQuery(q);
    }
    return std::nullopt;
}

bool DatabaseManager::touchItem(const QString& id) {
    QMutexLocker locker(&m_mutex);
    QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QSqlQuery q(m_db);
    q.prepare("UPDATE clipboard_items SET use_count = use_count + 1, updated_at = :ua WHERE id = :id");
    q.bindValue(":ua", now);
    q.bindValue(":id", id);
    return q.exec();
}

bool DatabaseManager::toggleFavorite(const QString& id) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("UPDATE clipboard_items SET is_favorite = CASE WHEN is_favorite = 1 THEN 0 ELSE 1 END WHERE id = :id");
    q.bindValue(":id", id);
    return q.exec();
}

bool DatabaseManager::togglePin(const QString& id) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("UPDATE clipboard_items SET is_pinned = CASE WHEN is_pinned = 1 THEN 0 ELSE 1 END WHERE id = :id");
    q.bindValue(":id", id);
    return q.exec();
}

bool DatabaseManager::updateNote(const QString& id, const QString& note) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("UPDATE clipboard_items SET note = :note WHERE id = :id");
    q.bindValue(":note", note.isEmpty() ? QVariant(QVariant::String) : note);
    q.bindValue(":id", id);
    return q.exec();
}

bool DatabaseManager::assignGroup(const QString& id, const QString& groupId) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("UPDATE clipboard_items SET group_id = :gid WHERE id = :id");
    q.bindValue(":gid", groupId.isEmpty() ? QVariant(QVariant::String) : groupId);
    q.bindValue(":id", id);
    return q.exec();
}

QList<ClipboardItem> DatabaseManager::queryItems(
    const QString& range,
    const QString& category,
    const QString& groupId,
    const QString& searchQuery,
    const QString& sortOrder,
    int limit,
    int offset
) {
    QMutexLocker locker(&m_mutex);
    QList<ClipboardItem> list;

    QString sql = 
        "SELECT i.*, a.name AS app_name, a.icon_file AS app_icon "
        "FROM clipboard_items i "
        "LEFT JOIN clipboard_apps a ON i.source_app_id = a.id ";

    QStringList conditions;
    QVariantMap bindings;

    if (range == "favorite") {
        conditions.append("i.is_favorite = 1");
    }

    if (category == "text") {
        conditions.append("i.kind IN ('text', 'html', 'rtf')");
    } else if (category == "image") {
        conditions.append("i.kind = 'image'");
    } else if (category == "files") {
        conditions.append("i.kind = 'files'");
    } else if (category == "link") {
        conditions.append("(i.sub_kind = 'url' OR i.search_text LIKE 'http://%' OR i.search_text LIKE 'https://%')");
    }

    if (!groupId.isEmpty() && groupId != "all" && groupId != "preserve") {
        if (groupId.startsWith("group:")) {
            QString realId = groupId.mid(6);
            conditions.append("i.group_id = :group_id");
            bindings[":group_id"] = realId;
        } else {
            conditions.append("i.group_id = :group_id");
            bindings[":group_id"] = groupId;
        }
    }

    if (!searchQuery.trimmed().isEmpty()) {
        conditions.append("(i.search_text LIKE :search OR i.note LIKE :search)");
        bindings[":search"] = QString("%%1%").arg(searchQuery.trimmed());
    }

    if (!conditions.isEmpty()) {
        sql += " WHERE " + conditions.join(" AND ");
    }

    // Always pinned items first, then sort order
    sql += " ORDER BY i.is_pinned DESC, ";
    if (sortOrder == "createdAtDesc") {
        sql += "i.created_at DESC";
    } else if (sortOrder == "useCountDesc") {
        sql += "i.use_count DESC, i.updated_at DESC";
    } else {
        sql += "i.updated_at DESC";
    }

    sql += QString(" LIMIT %1 OFFSET %2").arg(limit).arg(offset);

    QSqlQuery q(m_db);
    q.prepare(sql);
    for (auto it = bindings.begin(); it != bindings.end(); ++it) {
        q.bindValue(it.key(), it.value());
    }

    if (q.exec()) {
        while (q.next()) {
            list.append(itemFromQuery(q));
        }
    } else {
        Logger::error("queryItems error: " + q.lastError().text());
    }

    return list;
}

int DatabaseManager::countItems(
    const QString& range,
    const QString& category,
    const QString& groupId,
    const QString& searchQuery
) {
    QMutexLocker locker(&m_mutex);
    QString sql = "SELECT COUNT(*) FROM clipboard_items i ";
    QStringList conditions;
    QVariantMap bindings;

    if (range == "favorite") conditions.append("i.is_favorite = 1");
    if (category == "text") conditions.append("i.kind IN ('text', 'html', 'rtf')");
    else if (category == "image") conditions.append("i.kind = 'image'");
    else if (category == "files") conditions.append("i.kind = 'files'");
    else if (category == "link") conditions.append("(i.sub_kind = 'url' OR i.search_text LIKE 'http://%' OR i.search_text LIKE 'https://%')");

    if (!groupId.isEmpty() && groupId != "all" && groupId != "preserve") {
        QString realId = groupId.startsWith("group:") ? groupId.mid(6) : groupId;
        conditions.append("i.group_id = :group_id");
        bindings[":group_id"] = realId;
    }

    if (!searchQuery.trimmed().isEmpty()) {
        conditions.append("(i.search_text LIKE :search OR i.note LIKE :search)");
        bindings[":search"] = QString("%%1%").arg(searchQuery.trimmed());
    }

    if (!conditions.isEmpty()) sql += " WHERE " + conditions.join(" AND ");

    QSqlQuery q(m_db);
    q.prepare(sql);
    for (auto it = bindings.begin(); it != bindings.end(); ++it) {
        q.bindValue(it.key(), it.value());
    }

    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return 0;
}

QList<ClipboardGroup> DatabaseManager::listGroups(bool includeHidden) {
    QMutexLocker locker(&m_mutex);
    QList<ClipboardGroup> list;
    QString sql = "SELECT * FROM clipboard_groups";
    if (!includeHidden) {
        sql += " WHERE is_hidden = 0";
    }
    sql += " ORDER BY sort_order ASC, created_at ASC";

    QSqlQuery q(m_db);
    if (q.exec(sql)) {
        while (q.next()) {
            ClipboardGroup g;
            g.id = q.value("id").toString();
            g.name = q.value("name").toString();
            g.icon = q.value("icon").toString();
            g.isHidden = q.value("is_hidden").toInt() != 0;
            g.sortOrder = q.value("sort_order").toInt();
            g.createdAt = q.value("created_at").toString();
            g.updatedAt = q.value("updated_at").toString();
            list.append(g);
        }
    }
    return list;
}

bool DatabaseManager::createGroup(ClipboardGroup& group) {
    QMutexLocker locker(&m_mutex);
    if (group.id.isEmpty()) {
        group.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    group.createdAt = now;
    group.updatedAt = now;

    QSqlQuery q(m_db);
    q.prepare("INSERT INTO clipboard_groups (id, name, icon, is_hidden, sort_order, created_at, updated_at) "
              "VALUES (:id, :name, :icon, :is_hidden, :sort_order, :created_at, :updated_at)");
    q.bindValue(":id", group.id);
    q.bindValue(":name", group.name);
    q.bindValue(":icon", group.icon);
    q.bindValue(":is_hidden", group.isHidden ? 1 : 0);
    q.bindValue(":sort_order", group.sortOrder);
    q.bindValue(":created_at", group.createdAt);
    q.bindValue(":updated_at", group.updatedAt);
    return q.exec();
}

bool DatabaseManager::updateGroup(const ClipboardGroup& group) {
    QMutexLocker locker(&m_mutex);
    QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QSqlQuery q(m_db);
    q.prepare("UPDATE clipboard_groups SET name = :name, icon = :icon, is_hidden = :is_hidden, "
              "sort_order = :sort_order, updated_at = :updated_at WHERE id = :id");
    q.bindValue(":name", group.name);
    q.bindValue(":icon", group.icon);
    q.bindValue(":is_hidden", group.isHidden ? 1 : 0);
    q.bindValue(":sort_order", group.sortOrder);
    q.bindValue(":updated_at", now);
    q.bindValue(":id", group.id);
    return q.exec();
}

bool DatabaseManager::deleteGroup(const QString& id) {
    QMutexLocker locker(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM clipboard_groups WHERE id = :id");
    q.bindValue(":id", id);
    return q.exec();
}

bool DatabaseManager::updateGroupsLayout(const QStringList& order, const QStringList& visibleIds) {
    QMutexLocker locker(&m_mutex);
    QSet<QString> visibleSet(visibleIds.begin(), visibleIds.end());
    
    for (int i = 0; i < order.size(); ++i) {
        QString id = order[i];
        bool isHidden = !visibleSet.contains(id);
        QSqlQuery q(m_db);
        q.prepare("UPDATE clipboard_groups SET sort_order = :ord, is_hidden = :hid WHERE id = :id");
        q.bindValue(":ord", i);
        q.bindValue(":hid", isHidden ? 1 : 0);
        q.bindValue(":id", id);
        q.exec();
    }
    return true;
}

QList<ClipboardApp> DatabaseManager::listApps() {
    QMutexLocker locker(&m_mutex);
    QList<ClipboardApp> list;
    QSqlQuery q(m_db);
    if (q.exec("SELECT * FROM clipboard_apps ORDER BY name ASC")) {
        while (q.next()) {
            ClipboardApp a;
            a.id = q.value("id").toString();
            a.name = q.value("name").toString();
            a.iconFile = q.value("icon_file").toString();
            a.platform = q.value("platform").toString();
            a.createdAt = q.value("created_at").toString();
            a.updatedAt = q.value("updated_at").toString();
            list.append(a);
        }
    }
    return list;
}

bool DatabaseManager::createOrUpdateApp(const ClipboardApp& app) {
    QMutexLocker locker(&m_mutex);
    QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QSqlQuery q(m_db);
    q.prepare(
        "INSERT INTO clipboard_apps (id, name, icon_file, platform, created_at, updated_at) "
        "VALUES (:id, :name, :icon_file, :platform, :ca, :ua) "
        "ON CONFLICT(id) DO UPDATE SET "
        "  name = excluded.name, icon_file = COALESCE(excluded.icon_file, clipboard_apps.icon_file), "
        "  updated_at = excluded.updated_at"
    );
    q.bindValue(":id", app.id);
    q.bindValue(":name", app.name);
    q.bindValue(":icon_file", app.iconFile.isEmpty() ? QVariant(QVariant::String) : app.iconFile);
    q.bindValue(":platform", app.platform);
    q.bindValue(":ca", app.createdAt.isEmpty() ? now : app.createdAt);
    q.bindValue(":ua", now);
    return q.exec();
}

bool DatabaseManager::deleteUnreferencedApps(const QStringList& appIds) {
    QMutexLocker locker(&m_mutex);
    for (const QString& id : appIds) {
        QSqlQuery check(m_db);
        check.prepare("SELECT COUNT(*) FROM clipboard_items WHERE source_app_id = :id");
        check.bindValue(":id", id);
        if (check.exec() && check.next() && check.value(0).toInt() == 0) {
            QSqlQuery del(m_db);
            del.prepare("DELETE FROM clipboard_apps WHERE id = :id");
            del.bindValue(":id", id);
            del.exec();
        }
    }
    return true;
}

int DatabaseManager::cleanupOldHistory(int retentionDays, int maxCount) {
    QMutexLocker locker(&m_mutex);
    int deleted = 0;

    // 1. Retention Days (exclude favorite and pinned)
    if (retentionDays > 0) {
        QString cutoff = QDateTime::currentDateTimeUtc().addDays(-retentionDays).toString(Qt::ISODateWithMs);
        QSqlQuery q(m_db);
        q.prepare("DELETE FROM clipboard_items WHERE is_favorite = 0 AND is_pinned = 0 AND created_at < :cutoff");
        q.bindValue(":cutoff", cutoff);
        if (q.exec()) {
            deleted += q.numRowsAffected();
        }
    }

    // 2. Max Count (keep newest maxCount normal items, favorite & pinned preserved)
    if (maxCount > 0) {
        QSqlQuery q(m_db);
        QString sql = QString(
            "DELETE FROM clipboard_items WHERE id IN ("
            "  SELECT id FROM clipboard_items "
            "  WHERE is_favorite = 0 AND is_pinned = 0 "
            "  ORDER BY updated_at DESC "
            "  LIMIT -1 OFFSET %1"
            ")"
        ).arg(maxCount);
        if (q.exec(sql)) {
            deleted += q.numRowsAffected();
        }
    }

    return deleted;
}

qint64 DatabaseManager::cleanupUnreferencedCache() {
    QMutexLocker locker(&m_mutex);
    qint64 freedBytes = 0;

    // Get all image contents referenced in DB
    QSet<QString> referencedFiles;
    QSqlQuery q(m_db);
    if (q.exec("SELECT content FROM clipboard_items WHERE kind = 'image'")) {
        while (q.next()) {
            referencedFiles.insert(q.value(0).toString());
        }
    }

    // Clean image cache dir
    QDir imgDir(AppPaths::imageCacheDir());
    QFileInfoList fileList = imgDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi : fileList) {
        if (!referencedFiles.contains(fi.fileName())) {
            freedBytes += fi.size();
            QFile::remove(fi.absoluteFilePath());
        }
    }

    return freedBytes;
}

StorageUsage DatabaseManager::getStorageUsage() {
    StorageUsage usage;
    
    // DB size
    QFileInfo dbFi(AppPaths::databasePath());
    if (dbFi.exists()) {
        usage.databaseBytes = dbFi.size();
    }

    // Image cache size
    QDir imgDir(AppPaths::imageCacheDir());
    for (const QFileInfo& fi : imgDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        usage.imagesBytes += fi.size();
    }

    // Icon cache size
    QDir iconDir(AppPaths::iconCacheDir());
    for (const QFileInfo& fi : iconDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        usage.iconsBytes += fi.size();
    }

    usage.totalBytes = usage.databaseBytes + usage.imagesBytes + usage.iconsBytes;

    QSqlQuery q(m_db);
    if (q.exec("SELECT COUNT(*) FROM clipboard_items") && q.next()) {
        usage.itemCount = q.value(0).toInt();
    }

    return usage;
}

bool DatabaseManager::vacuum() {
    QMutexLocker locker(&m_mutex);
    QSqlQuery q(m_db);
    return q.exec("VACUUM");
}
