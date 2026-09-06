#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutex>

struct GeneralConfig {
    bool autoStart = false;
    bool runAsAdmin = false;
    bool trayIcon = true;
    bool dockIcon = false;
};

struct AppearanceConfig {
    QString theme = "auto";       // auto, light, dark
    QString language = "zh-CN";   // zh-CN, en-US
    QString accentColor = "#1677ff"; // Hex accent color (default Aurora Blue)
    QString fontFamily = "";      // Empty = System Default ("Microsoft YaHei UI")
    int opacity = 100;            // 75, 80, 85, 90, 95, 100 (default 100% opaque)
};

struct ShortcutsConfig {
    QString openClipboard = "Alt+C";
    QString openPreference = "Alt+X";
    bool winV = false;
};

struct CaptureConfig {
    bool text = true;
    bool html = true;
    bool rtf = true;
    bool image = true;
    bool files = true;
    int maxTextMb = 4;
    int maxImageMb = 100;
    QStringList order = {"files", "image", "html", "rtf", "text"};
};

struct FiltersConfig {
    QStringList excludedAppIds;
};

struct SensitiveConfig {
    bool collectSecrets = true;
    bool redactSecrets = true;
};

struct HistoryConfig {
    int retentionDays = 0;        // 0 = forever
    int maxCount = 0;             // 0 = unlimited
    int cleanupIntervalHours = 0; // 0 = only at startup
};

struct ContentConfig {
    bool autoFavorite = false;
    QString autoPaste = "doubleClickPaste"; // disabled, singleClickPaste, doubleClickPaste, singleClickCopy, doubleClickCopy
    QString middleClick = "disabled";       // disabled, singleClickPaste, singleClickPastePlain, singleClickCopy, singleClickCopyPlain
    bool copyPlain = false;
    bool copyThenHideWindow = false;
    bool pastePlain = false;
    bool pasteFilesAsPath = false;
    bool showOriginalPreview = true;
    bool deleteConfirm = true;
    bool deleteFavoriteItems = false;
    bool deleteFavoriteConfirm = true;
    bool deletePinnedItems = false;
    bool deletePinnedConfirm = true;
    bool deleteFavoriteItemsOnlyInFavoriteGroup = true;
    bool updateOnReuse = false;
    QString sort = "updatedAtDesc"; // updatedAtDesc, createdAtDesc, useCountDesc
    QStringList itemActions = {"copy", "star", "pinItem", "delete"};
    QStringList itemActionOrder = {
        "paste", "pastePlain", "pastePath", "copy", "copyPlain",
        "openLink", "sendEmail", "reveal", "note", "star", "pinItem", "delete"
    };
};

struct DisplayConfig {
    int textMaxLines = 3;
    int imageMaxHeight = 64;
    int fileMaxCount = 3;
};

struct PreviewConfig {
    bool hoverEnabled = false;
    int hoverDelayMs = 500;
    bool spaceEnabled = true;
};

struct SearchConfig {
    bool defaultFocus = false;
    bool clearOnHide = true;
};

struct WindowConfig {
    QString position = "followCursor"; // followCursor, center, remember
    bool scrollToTopOnOpen = true;
    QString selectRangeOnOpen = "preserve";       // preserve, all, favorite
    QString selectCategoryOnOpen = "preserve";    // preserve, all, text, image, files
    QString selectGroupOnOpen = "preserve";       // preserve, all, group:<id>
    bool lightweightMode = true;
    int idleDestroySeconds = 60;
    int lastWindowX = -1;
    int lastWindowY = -1;
    int windowWidth = 340;
    int windowHeight = 520;
};

struct FeedbackConfig {
    bool copySound = false;
};

struct UpdateConfig {
    bool autoCheck = true;
    QString frequency = "daily"; // daily, weekly, monthly
    bool includeBeta = false;
    bool includeNightly = false;
    QString lastCheckedAt;
    QString skippedVersion;
};

class AppConfig : public QObject {
    Q_OBJECT
public:
    static AppConfig* instance();

    void load();
    void save();
    void resetToDefaults();

    GeneralConfig general;
    AppearanceConfig appearance;
    ShortcutsConfig shortcuts;
    CaptureConfig capture;
    FiltersConfig filters;
    SensitiveConfig sensitive;
    HistoryConfig history;
    ContentConfig content;
    DisplayConfig display;
    PreviewConfig preview;
    SearchConfig search;
    WindowConfig window;
    FeedbackConfig feedback;
    UpdateConfig update;

signals:
    void configChanged(const QString& section);

private:
    explicit AppConfig(QObject* parent = nullptr);
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);

    mutable QRecursiveMutex m_mutex;
};
