#pragma once

#include <QIcon>
#include <QColor>

enum class FluentIconType {
    Record,
    History,
    Reuse,
    Workflow,
    Shortcuts,
    Data,
    About,
    Search,
    Pin,
    Settings,
    Star,
    Note,
    Trash,
    Copy,
    Paste,
    Folder,
    Globe,
    FileText,
    Image,
    Shield,
    Filter,
    Close
};

class FluentIcon {
public:
    static QIcon make(FluentIconType type, const QColor& color, int size = 20);
    static QIcon appIcon(int size = 64, bool filled = true);
    static QIcon trayIcon(int size = 22);
};
