#include "HotkeyManager.h"
#include "AppConfig.h"
#include "EventBus.h"
#include "Logger.h"

HotkeyManager* HotkeyManager::instance() {
    static HotkeyManager s_instance;
    return &s_instance;
}

HotkeyManager::HotkeyManager(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_DontShowOnScreen);
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
}

HotkeyManager::~HotkeyManager() {
    unregisterHotkeys();
}

void HotkeyManager::registerHotkeys() {
    unregisterHotkeys();
    auto config = AppConfig::instance();

    registerSingleHotkey(ID_OPEN_CLIPBOARD, config->shortcuts.openClipboard);
    registerSingleHotkey(ID_OPEN_PREFERENCE, config->shortcuts.openPreference);
}

void HotkeyManager::unregisterHotkeys() {
    HWND hwnd = reinterpret_cast<HWND>(winId());
    UnregisterHotKey(hwnd, ID_OPEN_CLIPBOARD);
    UnregisterHotKey(hwnd, ID_OPEN_PREFERENCE);
}

bool HotkeyManager::registerSingleHotkey(int id, const QString& sequenceStr) {
    if (sequenceStr.trimmed().isEmpty()) return false;

    QKeySequence seq(sequenceStr);
    if (seq.isEmpty()) return false;

    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    auto keyComb = seq[0];
    auto mods = keyComb.keyboardModifiers();
    auto key = keyComb.key();
    #else
    int keyComb = seq[0];
    int mods = keyComb & Qt::KeyboardModifierMask;
    int key = keyComb & ~Qt::KeyboardModifierMask;
    #endif

    UINT winMods = 0;
    if (mods & Qt::AltModifier) winMods |= MOD_ALT;
    if (mods & Qt::ControlModifier) winMods |= MOD_CONTROL;
    if (mods & Qt::ShiftModifier) winMods |= MOD_SHIFT;
    if (mods & Qt::MetaModifier) winMods |= MOD_WIN;

    UINT vk = 0;
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        vk = 'A' + (key - Qt::Key_A);
    } else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        vk = '0' + (key - Qt::Key_0);
    } else if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        vk = VK_F1 + (key - Qt::Key_F1);
    } else if (key == Qt::Key_Space) {
        vk = VK_SPACE;
    } else {
        vk = key; // fallback
    }

    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (RegisterHotKey(hwnd, id, winMods | MOD_NOREPEAT, vk)) {
        Logger::info(QString("Registered global hotkey %1 -> %2").arg(id).arg(sequenceStr));
        return true;
    } else {
        Logger::warn(QString("Failed to register global hotkey: %1").arg(sequenceStr));
        return false;
    }
}

bool HotkeyManager::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    Q_UNUSED(eventType)
    Q_UNUSED(result)

    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY) {
        int id = static_cast<int>(msg->wParam);
        if (id == ID_OPEN_CLIPBOARD) {
            EventBus::instance()->toggleClipboardWindow();
            return true;
        } else if (id == ID_OPEN_PREFERENCE) {
            EventBus::instance()->showPreferencesWindow();
            return true;
        }
    }

    return false;
}
