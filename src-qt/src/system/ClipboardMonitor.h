#pragma once

#include <QObject>
#include <QClipboard>
#include "Models.h"

class ClipboardMonitor : public QObject {
    Q_OBJECT
public:
    static ClipboardMonitor* instance();

    void start();
    void stop();
    void setSuppressNext(bool suppress) { m_suppressNext = suppress; }

signals:
    void itemCaptured(const ClipboardItem& item);

private slots:
    void processClipboardChange();

private:
    explicit ClipboardMonitor(QObject* parent = nullptr);
    ~ClipboardMonitor();

    ClipboardApp detectActiveApp();

    bool m_listening = false;
    bool m_suppressNext = false;
};
