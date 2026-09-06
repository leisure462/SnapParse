#pragma once

#include <QObject>
#include <windows.h>
#include "Models.h"

class PasteInjector : public QObject {
    Q_OBJECT
public:
    static PasteInjector* instance();

    void recordTargetWindow();
    void writeToClipboard(const ClipboardItem& item, bool plain = false, bool filesAsPath = false);
    void pasteItem(const ClipboardItem& item, bool plain = false, bool filesAsPath = false);

private:
    explicit PasteInjector(QObject* parent = nullptr) : QObject(parent) {}

    HWND m_targetWindow = nullptr;
};
