#pragma once

#include <QWidget>
#include <QKeySequence>
#include <windows.h>

class HotkeyManager : public QWidget {
    Q_OBJECT
public:
    static HotkeyManager* instance();

    void registerHotkeys();
    void unregisterHotkeys();

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    explicit HotkeyManager(QWidget* parent = nullptr);
    ~HotkeyManager();

    bool registerSingleHotkey(int id, const QString& sequenceStr);

    enum HotkeyId {
        ID_OPEN_CLIPBOARD = 1001,
        ID_OPEN_PREFERENCE = 1002
    };
};
