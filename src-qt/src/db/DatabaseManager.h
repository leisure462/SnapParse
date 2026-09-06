#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMutex>
#include "Models.h"

class DatabaseManager : public QObject {
    Q_OBJECT
public:
    static DatabaseManager* instance();

    bool init(const QString& dbPath = "");
    void close();
    bool reconnect(const QString& newDbPath);

    // Items CRUD
    bool insertItem(ClipboardItem& item);
    bool updateItem(const ClipboardItem& item);
    bool deleteItem(const QString& id);
    std::optional<ClipboardItem> getItem(const QString& id);
    std::optional<ClipboardItem> findItemByHash(const QString& hash);
    bool touchItem(const QString& id);
    bool toggleFavorite(const QString& id);
    bool togglePin(const QString& id);
    bool updateNote(const QString& id, const QString& note);
    bool assignGroup(const QString& id, const QString& groupId);

    // Queries
    QList<ClipboardItem> queryItems(
        const QString& range = "all",         // "all", "favorite"
        const QString& category = "all",      // "all", "text", "image", "files"
        const QString& groupId = "",          // "", "all", "group:<id>"
        const QString& searchQuery = "",
        const QString& sortOrder = "updatedAtDesc",
        int limit = 100,
        int offset = 0
    );
    int countItems(
        const QString& range = "all",
        const QString& category = "all",
        const QString& groupId = "",
        const QString& searchQuery = ""
    );

    // Groups CRUD
    QList<ClipboardGroup> listGroups(bool includeHidden = true);
    bool createGroup(ClipboardGroup& group);
    bool updateGroup(const ClipboardGroup& group);
    bool deleteGroup(const QString& id);
    bool updateGroupsLayout(const QStringList& order, const QStringList& visibleIds);

    // Apps CRUD
    QList<ClipboardApp> listApps();
    bool createOrUpdateApp(const ClipboardApp& app);
    bool deleteUnreferencedApps(const QStringList& appIds);

    // Cleanup & Stats
    int cleanupOldHistory(int retentionDays, int maxCount);
    qint64 cleanupUnreferencedCache();
    bool vacuum();
    StorageUsage getStorageUsage();

private:
    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager();

    bool createTables();
    ClipboardItem itemFromQuery(const QSqlQuery& query);

    QSqlDatabase m_db;
    QRecursiveMutex m_mutex;
    QString m_connectionName;
};
