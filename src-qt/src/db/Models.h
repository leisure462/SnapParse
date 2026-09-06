#pragma once

#include <QString>
#include <QStringList>
#include <QDateTime>

struct ClipboardItem {
    QString id;
    QString kind;         // "text", "html", "rtf", "image", "files"
    QString subKind;      // "url", "email", "color", "path", etc.
    QString groupId;
    QString sourceAppId;
    QString content;
    QString contentHash;
    QString searchText;
    QString summary;
    QString fileTypes;
    qint64 size = 0;
    int width = 0;
    int height = 0;
    int useCount = 1;
    bool isFavorite = false;
    bool isPinned = false;
    bool isSensitive = false;
    QString platform = "windows";
    QString note;
    QString createdAt;
    QString updatedAt;

    // Joined fields
    QString sourceAppName;
    QString sourceAppIcon;
};

struct ClipboardGroup {
    QString id;
    QString name;
    QString icon = "folder";
    bool isHidden = false;
    int sortOrder = 0;
    QString createdAt;
    QString updatedAt;
};

struct ClipboardApp {
    QString id;
    QString name;
    QString iconFile;
    QString platform = "windows";
    QString createdAt;
    QString updatedAt;
};

struct StorageUsage {
    qint64 totalBytes = 0;
    qint64 databaseBytes = 0;
    qint64 imagesBytes = 0;
    qint64 iconsBytes = 0;
    int itemCount = 0;
};
