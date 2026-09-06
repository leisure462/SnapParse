#include "AppConfig.h"
#include "AppPaths.h"
#include "Logger.h"
#include "EventBus.h"
#include <QFile>
#include <QDir>

AppConfig* AppConfig::instance() {
    static AppConfig s_instance;
    return &s_instance;
}

AppConfig::AppConfig(QObject* parent) : QObject(parent) {
    load();
}

void AppConfig::resetToDefaults() {
    QMutexLocker locker(&m_mutex);
    general = GeneralConfig();
    appearance = AppearanceConfig();
    shortcuts = ShortcutsConfig();
    capture = CaptureConfig();
    filters = FiltersConfig();
    sensitive = SensitiveConfig();
    history = HistoryConfig();
    content = ContentConfig();
    display = DisplayConfig();
    preview = PreviewConfig();
    search = SearchConfig();
    window = WindowConfig();
    feedback = FeedbackConfig();
    update = UpdateConfig();
    
    save();
    emit configChanged("all");
    EventBus::instance()->settingsChanged("all");
}

void AppConfig::load() {
    QMutexLocker locker(&m_mutex);
    QString path = AppPaths::appDataDir() + "/config.json";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        save(); // create default
        return;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject()) {
        fromJson(doc.object());
    }
}

void AppConfig::save() {
    QMutexLocker locker(&m_mutex);
    QString path = AppPaths::appDataDir() + "/config.json";
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonDocument doc(toJson());
        file.write(doc.toJson(QJsonDocument::Indented));
    }
}

QJsonObject AppConfig::toJson() const {
    QJsonObject root;

    // General
    QJsonObject g;
    g["autoStart"] = general.autoStart;
    g["runAsAdmin"] = general.runAsAdmin;
    g["trayIcon"] = general.trayIcon;
    g["dockIcon"] = general.dockIcon;
    root["general"] = g;

    // Appearance
    QJsonObject a;
    a["theme"] = appearance.theme;
    a["language"] = appearance.language;
    a["accentColor"] = appearance.accentColor;
    root["appearance"] = a;

    // Shortcuts
    QJsonObject sc;
    sc["openClipboard"] = shortcuts.openClipboard;
    sc["openPreference"] = shortcuts.openPreference;
    sc["winV"] = shortcuts.winV;
    root["shortcuts"] = sc;

    // Clipboard
    QJsonObject cb;
    
    // Capture
    QJsonObject cap;
    cap["text"] = capture.text;
    cap["html"] = capture.html;
    cap["rtf"] = capture.rtf;
    cap["image"] = capture.image;
    cap["files"] = capture.files;
    cap["maxTextMb"] = capture.maxTextMb;
    cap["maxImageMb"] = capture.maxImageMb;
    QJsonArray capOrder;
    for (const QString& k : capture.order) capOrder.append(k);
    cap["order"] = capOrder;
    cb["capture"] = cap;

    // Filters
    QJsonObject flt;
    QJsonArray excApps;
    for (const QString& id : filters.excludedAppIds) excApps.append(id);
    flt["excludedAppIds"] = excApps;
    cb["filters"] = flt;

    // Sensitive
    QJsonObject sen;
    sen["collectSecrets"] = sensitive.collectSecrets;
    sen["redactSecrets"] = sensitive.redactSecrets;
    cb["sensitive"] = sen;

    // History
    QJsonObject h;
    h["retentionDays"] = history.retentionDays;
    h["maxCount"] = history.maxCount;
    h["cleanupIntervalHours"] = history.cleanupIntervalHours;
    cb["history"] = h;

    // Content
    QJsonObject cnt;
    cnt["autoFavorite"] = content.autoFavorite;
    cnt["autoPaste"] = content.autoPaste;
    cnt["middleClick"] = content.middleClick;
    cnt["copyPlain"] = content.copyPlain;
    cnt["copyThenHideWindow"] = content.copyThenHideWindow;
    cnt["pastePlain"] = content.pastePlain;
    cnt["pasteFilesAsPath"] = content.pasteFilesAsPath;
    cnt["showOriginalPreview"] = content.showOriginalPreview;
    cnt["deleteConfirm"] = content.deleteConfirm;
    cnt["deleteFavoriteItems"] = content.deleteFavoriteItems;
    cnt["deleteFavoriteConfirm"] = content.deleteFavoriteConfirm;
    cnt["deletePinnedItems"] = content.deletePinnedItems;
    cnt["deletePinnedConfirm"] = content.deletePinnedConfirm;
    cnt["deleteFavoriteItemsOnlyInFavoriteGroup"] = content.deleteFavoriteItemsOnlyInFavoriteGroup;
    cnt["updateOnReuse"] = content.updateOnReuse;
    cnt["sort"] = content.sort;
    QJsonArray act;
    for (const QString& a : content.itemActions) act.append(a);
    cnt["itemActions"] = act;
    QJsonArray actOrd;
    for (const QString& a : content.itemActionOrder) actOrd.append(a);
    cnt["itemActionOrder"] = actOrd;
    cb["content"] = cnt;

    // Display
    QJsonObject d;
    d["textMaxLines"] = display.textMaxLines;
    d["imageMaxHeight"] = display.imageMaxHeight;
    d["fileMaxCount"] = display.fileMaxCount;
    cb["display"] = d;

    // Preview
    QJsonObject prv;
    prv["hoverEnabled"] = preview.hoverEnabled;
    prv["hoverDelayMs"] = preview.hoverDelayMs;
    prv["spaceEnabled"] = preview.spaceEnabled;
    cb["preview"] = prv;

    // Search
    QJsonObject srch;
    srch["defaultFocus"] = search.defaultFocus;
    srch["clearOnHide"] = search.clearOnHide;
    cb["search"] = srch;

    // Window
    QJsonObject w;
    w["position"] = window.position;
    w["scrollToTopOnOpen"] = window.scrollToTopOnOpen;
    w["selectRangeOnOpen"] = window.selectRangeOnOpen;
    w["selectCategoryOnOpen"] = window.selectCategoryOnOpen;
    w["selectGroupOnOpen"] = window.selectGroupOnOpen;
    w["lightweightMode"] = window.lightweightMode;
    w["idleDestroySeconds"] = window.idleDestroySeconds;
    w["lastWindowX"] = window.lastWindowX;
    w["lastWindowY"] = window.lastWindowY;
    w["windowWidth"] = window.windowWidth;
    w["windowHeight"] = window.windowHeight;
    cb["window"] = w;

    // Feedback
    QJsonObject fb;
    fb["copySound"] = feedback.copySound;
    cb["feedback"] = fb;

    root["clipboard"] = cb;

    // Update
    QJsonObject upd;
    upd["autoCheck"] = update.autoCheck;
    upd["frequency"] = update.frequency;
    upd["includeBeta"] = update.includeBeta;
    upd["includeNightly"] = update.includeNightly;
    upd["lastCheckedAt"] = update.lastCheckedAt;
    upd["skippedVersion"] = update.skippedVersion;
    root["update"] = upd;

    return root;
}

void AppConfig::fromJson(const QJsonObject& root) {
    // General
    if (root.contains("general")) {
        QJsonObject g = root["general"].toObject();
        general.autoStart = g.value("autoStart").toBool(general.autoStart);
        general.runAsAdmin = g.value("runAsAdmin").toBool(general.runAsAdmin);
        general.trayIcon = g.value("trayIcon").toBool(general.trayIcon);
        general.dockIcon = g.value("dockIcon").toBool(general.dockIcon);
    }

    // Appearance
    if (root.contains("appearance")) {
        QJsonObject a = root["appearance"].toObject();
        appearance.theme = a.value("theme").toString(appearance.theme);
        appearance.language = a.value("language").toString(appearance.language);
        appearance.accentColor = a.value("accentColor").toString(appearance.accentColor);
    }

    // Shortcuts
    if (root.contains("shortcuts")) {
        QJsonObject sc = root["shortcuts"].toObject();
        shortcuts.openClipboard = sc.value("openClipboard").toString(shortcuts.openClipboard);
        shortcuts.openPreference = sc.value("openPreference").toString(shortcuts.openPreference);
        shortcuts.winV = sc.value("winV").toBool(shortcuts.winV);
    }

    // Clipboard
    if (root.contains("clipboard")) {
        QJsonObject cb = root["clipboard"].toObject();

        if (cb.contains("capture")) {
            QJsonObject cap = cb["capture"].toObject();
            capture.text = cap.value("text").toBool(capture.text);
            capture.html = cap.value("html").toBool(capture.html);
            capture.rtf = cap.value("rtf").toBool(capture.rtf);
            capture.image = cap.value("image").toBool(capture.image);
            capture.files = cap.value("files").toBool(capture.files);
            capture.maxTextMb = cap.value("maxTextMb").toInt(capture.maxTextMb);
            capture.maxImageMb = cap.value("maxImageMb").toInt(capture.maxImageMb);
            if (cap.contains("order")) {
                capture.order.clear();
                for (const auto& v : cap["order"].toArray()) capture.order.append(v.toString());
            }
        }

        if (cb.contains("filters")) {
            QJsonObject flt = cb["filters"].toObject();
            if (flt.contains("excludedAppIds")) {
                filters.excludedAppIds.clear();
                for (const auto& v : flt["excludedAppIds"].toArray()) filters.excludedAppIds.append(v.toString());
            }
        }

        if (cb.contains("sensitive")) {
            QJsonObject sen = cb["sensitive"].toObject();
            sensitive.collectSecrets = sen.value("collectSecrets").toBool(sensitive.collectSecrets);
            sensitive.redactSecrets = sen.value("redactSecrets").toBool(sensitive.redactSecrets);
        }

        if (cb.contains("history")) {
            QJsonObject h = cb["history"].toObject();
            history.retentionDays = h.value("retentionDays").toInt(history.retentionDays);
            history.maxCount = h.value("maxCount").toInt(history.maxCount);
            history.cleanupIntervalHours = h.value("cleanupIntervalHours").toInt(history.cleanupIntervalHours);
        }

        if (cb.contains("content")) {
            QJsonObject cnt = cb["content"].toObject();
            content.autoFavorite = cnt.value("autoFavorite").toBool(content.autoFavorite);
            content.autoPaste = cnt.value("autoPaste").toString(content.autoPaste);
            content.middleClick = cnt.value("middleClick").toString(content.middleClick);
            content.copyPlain = cnt.value("copyPlain").toBool(content.copyPlain);
            content.copyThenHideWindow = cnt.value("copyThenHideWindow").toBool(content.copyThenHideWindow);
            content.pastePlain = cnt.value("pastePlain").toBool(content.pastePlain);
            content.pasteFilesAsPath = cnt.value("pasteFilesAsPath").toBool(content.pasteFilesAsPath);
            content.showOriginalPreview = cnt.value("showOriginalPreview").toBool(content.showOriginalPreview);
            content.deleteConfirm = cnt.value("deleteConfirm").toBool(content.deleteConfirm);
            content.deleteFavoriteItems = cnt.value("deleteFavoriteItems").toBool(content.deleteFavoriteItems);
            content.deleteFavoriteConfirm = cnt.value("deleteFavoriteConfirm").toBool(content.deleteFavoriteConfirm);
            content.deletePinnedItems = cnt.value("deletePinnedItems").toBool(content.deletePinnedItems);
            content.deletePinnedConfirm = cnt.value("deletePinnedConfirm").toBool(content.deletePinnedConfirm);
            content.deleteFavoriteItemsOnlyInFavoriteGroup = cnt.value("deleteFavoriteItemsOnlyInFavoriteGroup").toBool(content.deleteFavoriteItemsOnlyInFavoriteGroup);
            content.updateOnReuse = cnt.value("updateOnReuse").toBool(content.updateOnReuse);
            content.sort = cnt.value("sort").toString(content.sort);
            if (cnt.contains("itemActions")) {
                content.itemActions.clear();
                for (const auto& v : cnt["itemActions"].toArray()) content.itemActions.append(v.toString());
            }
            if (cnt.contains("itemActionOrder")) {
                content.itemActionOrder.clear();
                for (const auto& v : cnt["itemActionOrder"].toArray()) content.itemActionOrder.append(v.toString());
            }
        }

        if (cb.contains("display")) {
            QJsonObject d = cb["display"].toObject();
            display.textMaxLines = d.value("textMaxLines").toInt(display.textMaxLines);
            display.imageMaxHeight = d.value("imageMaxHeight").toInt(display.imageMaxHeight);
            display.fileMaxCount = d.value("fileMaxCount").toInt(display.fileMaxCount);
        }

        if (cb.contains("preview")) {
            QJsonObject prv = cb["preview"].toObject();
            preview.hoverEnabled = prv.value("hoverEnabled").toBool(preview.hoverEnabled);
            preview.hoverDelayMs = prv.value("hoverDelayMs").toInt(preview.hoverDelayMs);
            preview.spaceEnabled = prv.value("spaceEnabled").toBool(preview.spaceEnabled);
        }

        if (cb.contains("search")) {
            QJsonObject srch = cb["search"].toObject();
            search.defaultFocus = srch.value("defaultFocus").toBool(search.defaultFocus);
            search.clearOnHide = srch.value("clearOnHide").toBool(search.clearOnHide);
        }

        if (cb.contains("window")) {
            QJsonObject w = cb["window"].toObject();
            window.position = w.value("position").toString(window.position);
            window.scrollToTopOnOpen = w.value("scrollToTopOnOpen").toBool(window.scrollToTopOnOpen);
            window.selectRangeOnOpen = w.value("selectRangeOnOpen").toString(window.selectRangeOnOpen);
            window.selectCategoryOnOpen = w.value("selectCategoryOnOpen").toString(window.selectCategoryOnOpen);
            window.selectGroupOnOpen = w.value("selectGroupOnOpen").toString(window.selectGroupOnOpen);
            window.lightweightMode = w.value("lightweightMode").toBool(window.lightweightMode);
            window.idleDestroySeconds = w.value("idleDestroySeconds").toInt(window.idleDestroySeconds);
            window.lastWindowX = w.value("lastWindowX").toInt(window.lastWindowX);
            window.lastWindowY = w.value("lastWindowY").toInt(window.lastWindowY);
            window.windowWidth = w.value("windowWidth").toInt(window.windowWidth);
            window.windowHeight = w.value("windowHeight").toInt(window.windowHeight);
        }

        if (cb.contains("feedback")) {
            QJsonObject fb = cb["feedback"].toObject();
            feedback.copySound = fb.value("copySound").toBool(feedback.copySound);
        }
    }

    // Update
    if (root.contains("update")) {
        QJsonObject upd = root["update"].toObject();
        update.autoCheck = upd.value("autoCheck").toBool(update.autoCheck);
        update.frequency = upd.value("frequency").toString(update.frequency);
        update.includeBeta = upd.value("includeBeta").toBool(update.includeBeta);
        update.includeNightly = upd.value("includeNightly").toBool(update.includeNightly);
        update.lastCheckedAt = upd.value("lastCheckedAt").toString(update.lastCheckedAt);
        update.skippedVersion = upd.value("skippedVersion").toString(update.skippedVersion);
    }
}
