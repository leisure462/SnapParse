#include "ThemeManager.h"
#include "AppConfig.h"
#include "EventBus.h"
#include <QApplication>
#include <QSettings>
#include <QPalette>
#include <QFont>

namespace {
int interpolateAlpha(int opacity, int a75, int a100 = 255) {
    if (opacity <= 75) return a75;
    if (opacity >= 100) return a100;
    return a75 + (a100 - a75) * (opacity - 75) / 25;
}
}

ThemeManager* ThemeManager::instance() {
    static ThemeManager s_instance;
    return &s_instance;
}

ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {
    m_fontFamily = AppConfig::instance()->appearance.fontFamily;
    m_clipboardOpacity = AppConfig::instance()->appearance.opacity;
    if (m_clipboardOpacity < 75 || m_clipboardOpacity > 100) m_clipboardOpacity = 100;
}

bool ThemeManager::detectSystemDarkMode() const {
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat);
    return settings.value("AppsUseLightTheme", 1).toInt() == 0;
}

void ThemeManager::applyTheme(const QString& themeSetting) {
    QString theme = themeSetting;
    if (theme.isEmpty()) {
        theme = AppConfig::instance()->appearance.theme;
    }

    if (m_fontFamily.isEmpty()) {
        m_fontFamily = AppConfig::instance()->appearance.fontFamily;
    }

    // Set application font
    QFont appFont = qApp->font();
    if (m_fontFamily.isEmpty() || m_fontFamily == "default") {
        appFont.setFamily("Microsoft YaHei UI");
    } else {
        appFont.setFamily(m_fontFamily);
    }
    appFont.setStyleHint(QFont::SansSerif);
    appFont.setStyleStrategy(QFont::PreferAntialias);
    qApp->setFont(appFont);

    if (theme == "dark") {
        m_isDark = true;
    } else if (theme == "light") {
        m_isDark = false;
    } else {
        m_isDark = detectSystemDarkMode();
    }

    // Set Global Application Palette
    QPalette pal;
    pal.setColor(QPalette::Window, backgroundColor());
    pal.setColor(QPalette::WindowText, textColor());
    pal.setColor(QPalette::Base, cardBackgroundColor());
    pal.setColor(QPalette::AlternateBase, hoverColor());
    pal.setColor(QPalette::ToolTipBase, cardBackgroundColor());
    pal.setColor(QPalette::ToolTipText, textColor());
    pal.setColor(QPalette::Text, textColor());
    pal.setColor(QPalette::Button, cardBackgroundColor());
    pal.setColor(QPalette::ButtonText, textColor());
    pal.setColor(QPalette::Highlight, primaryColor());
    pal.setColor(QPalette::HighlightedText, Qt::white);
    qApp->setPalette(pal);

    // Apply Global Style Sheet
    qApp->setStyleSheet(generateStyleSheet());

    emit themeApplied();
    EventBus::instance()->themeChanged(m_isDark ? "dark" : "light");
}

void ThemeManager::setAccentColor(const QString& hexColor) {
    if (QColor::isValidColorName(hexColor)) {
        AppConfig::instance()->appearance.accentColor = hexColor;
        AppConfig::instance()->save();
        applyTheme(AppConfig::instance()->appearance.theme);
    }
}

void ThemeManager::setFontFamily(const QString& family) {
    if (m_fontFamily == family) {
        return;
    }
    m_fontFamily = family;
    AppConfig::instance()->appearance.fontFamily = family;
    AppConfig::instance()->save();
    applyTheme(AppConfig::instance()->appearance.theme);
}

void ThemeManager::setClipboardOpacity(int opacity) {
    opacity = qBound(75, opacity, 100);
    if (m_clipboardOpacity == opacity) {
        return;
    }
    m_clipboardOpacity = opacity;
    AppConfig::instance()->appearance.opacity = opacity;
    AppConfig::instance()->save();
    emit themeApplied();
}

qreal ThemeManager::targetWindowOpacity() const {
    int op = qBound(75, m_clipboardOpacity, 100);
    return op / 100.0;
}

QColor ThemeManager::backgroundColor() const {
    return m_isDark ? QColor("#141416") : QColor("#f4f5f7");
}

QColor ThemeManager::cardBackgroundColor() const {
    return m_isDark ? QColor("#222226") : QColor("#ffffff");
}

QColor ThemeManager::textColor() const {
    return m_isDark ? QColor("#f5f5f7") : QColor("#1d1d1f");
}

QColor ThemeManager::secondaryTextColor() const {
    return m_isDark ? QColor("#98989f") : QColor("#86868b");
}

QColor ThemeManager::primaryColor() const {
    QString hex = AppConfig::instance()->appearance.accentColor;
    if (QColor::isValidColorName(hex)) {
        return QColor(hex);
    }
    return QColor("#1677ff"); // Default Aurora Blue
}

QColor ThemeManager::borderColor() const {
    return m_isDark ? QColor(255, 255, 255, 26) : QColor(0, 0, 0, 18);
}

QColor ThemeManager::hoverColor() const {
    return m_isDark ? QColor(255, 255, 255, 14) : QColor(0, 0, 0, 8);
}

// Frosted Glass Material Colors
QColor ThemeManager::windowGlassColor() const {
    int a = m_isDark 
        ? interpolateAlpha(m_clipboardOpacity, 180, 255)
        : interpolateAlpha(m_clipboardOpacity, 190, 255);
    return m_isDark ? QColor(22, 22, 26, a) : QColor(246, 247, 250, a);
}

QColor ThemeManager::cardGlassColor() const {
    int a = m_isDark
        ? interpolateAlpha(m_clipboardOpacity, 200, 255)
        : interpolateAlpha(m_clipboardOpacity, 215, 255);
    return m_isDark ? QColor(36, 36, 42, a) : QColor(255, 255, 255, a);
}

QColor ThemeManager::cardGlassHoverColor() const {
    int a = m_isDark
        ? interpolateAlpha(m_clipboardOpacity, 225, 255)
        : interpolateAlpha(m_clipboardOpacity, 245, 255);
    return m_isDark ? QColor(52, 52, 60, a) : QColor(255, 255, 255, a);
}

QColor ThemeManager::cardGlassSelectedColor() const {
    QColor p = primaryColor();
    int a = m_isDark
        ? interpolateAlpha(m_clipboardOpacity, 215, 255)
        : interpolateAlpha(m_clipboardOpacity, 230, 255);
    if (m_isDark) {
        return QColor(
            (36 * 70 + p.red() * 30) / 100,
            (36 * 70 + p.green() * 30) / 100,
            (42 * 70 + p.blue() * 30) / 100,
            a
        );
    } else {
        return QColor(
            (255 * 88 + p.red() * 12) / 100,
            (255 * 88 + p.green() * 12) / 100,
            (255 * 88 + p.blue() * 12) / 100,
            a
        );
    }
}

QColor ThemeManager::glassBorderColor() const {
    return m_isDark ? QColor(255, 255, 255, 32) : QColor(0, 0, 0, 20);
}

QColor ThemeManager::glassInnerGlowColor() const {
    return m_isDark ? QColor(255, 255, 255, 12) : QColor(255, 255, 255, 180);
}

QString ThemeManager::generateStyleSheet() {
    QString text = m_isDark ? "#f5f5f7" : "#1d1d1f";
    QString bg = m_isDark ? "#1c1c20" : "#f4f4f7";
    QString secText = m_isDark ? "#98989f" : "#86868b";
    QString cardBg = m_isDark ? "rgba(255, 255, 255, 0.06)" : "rgba(255, 255, 255, 0.85)";
    QString border = m_isDark ? "rgba(255, 255, 255, 0.15)" : "rgba(0, 0, 0, 0.12)";
    QString hover = m_isDark ? "rgba(255, 255, 255, 0.10)" : "rgba(0, 0, 0, 0.05)";
    QString menuBg = m_isDark ? "#232328" : "#ffffff";
    QString menuBorder = m_isDark ? "rgba(255, 255, 255, 0.14)" : "rgba(0, 0, 0, 0.12)";
    QString menuHover = m_isDark ? "rgba(255, 255, 255, 0.10)" : "rgba(0, 0, 0, 0.06)";
    QString menuSep = m_isDark ? "rgba(255, 255, 255, 0.10)" : "rgba(0, 0, 0, 0.08)";
    QString primary = primaryColor().name();

    QString arrowDown = m_isDark ? ":/icons/chevron_down_dark.png" : ":/icons/chevron_down_light.png";
    QString arrowUp = m_isDark ? ":/icons/chevron_up_dark.png" : ":/icons/chevron_up_light.png";

    QString fontFam = (m_fontFamily.isEmpty() || m_fontFamily == "default")
                      ? QString("\"Microsoft YaHei UI\", sans-serif")
                      : QString("\"%1\", \"Microsoft YaHei UI\", sans-serif").arg(m_fontFamily);

    return QString(R"(
        QWidget {
            font-family: )" + fontFam + R"(;
            color: %1;
            font-size: 13px;
        }
        PreferencesWindow, QMainWindow, QDialog {
            background-color: %2;
        }
        QScrollArea, QScrollArea > QWidget > QWidget {
            background-color: transparent;
            border: none;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 4px;
            margin: 4px 1px 4px 0px;
        }
        QScrollBar::handle:vertical {
            background: %3;
            min-height: 28px;
            border-radius: 2px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QGroupBox {
            font-weight: 600;
            font-size: 13px;
            color: %1;
            border: 1px solid %5;
            border-radius: 10px;
            margin-top: 14px;
            padding-top: 14px;
            background-color: %4;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 14px;
            padding: 0 4px;
            color: %1;
            background-color: transparent;
        }
        QPushButton {
            background-color: %4;
            border: 1px solid %5;
            border-radius: 6px;
            padding: 4px 14px;
            color: %1;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: %6;
            border-color: %7;
        }
        QPushButton:pressed {
            background-color: %5;
        }
        QPushButton:disabled {
            color: %3;
            border-color: %5;
        }
        QLineEdit {
            background-color: %4;
            border: 1px solid %5;
            border-radius: 6px;
            padding: 5px 10px;
            color: %1;
        }
        QLineEdit:focus {
            border-color: %7;
        }
        QSpinBox {
            background-color: %4;
            border: 1px solid %5;
            border-radius: 6px;
            padding: 3px 22px 3px 10px;
            color: %1;
            font-size: 13px;
        }
        QSpinBox:hover {
            background-color: %6;
        }
        QSpinBox:focus {
            border-color: %7;
        }
        QSpinBox::up-button {
            subcontrol-origin: border;
            subcontrol-position: top right;
            width: 18px;
            height: 15px;
            border-left: 1px solid %5;
            border-bottom: 1px solid %5;
            border-top-right-radius: 5px;
            background: transparent;
        }
        QSpinBox::up-button:hover {
            background-color: %6;
        }
        QSpinBox::down-button {
            subcontrol-origin: border;
            subcontrol-position: bottom right;
            width: 18px;
            height: 15px;
            border-left: 1px solid %5;
            border-bottom-right-radius: 5px;
            background: transparent;
        }
        QSpinBox::down-button:hover {
            background-color: %6;
        }
        QSpinBox::up-arrow {
            image: url()" + arrowUp + R"();
            width: 8px;
            height: 8px;
        }
        QSpinBox::down-arrow {
            image: url()" + arrowDown + R"();
            width: 8px;
            height: 8px;
        }
        QComboBox {
            background-color: %4;
            border: 1px solid %5;
            border-radius: 6px;
            padding: 4px 26px 4px 10px;
            color: %1;
            min-width: 100px;
        }
        QComboBox:hover {
            background-color: %6;
        }
        QComboBox:focus {
            border-color: %7;
        }
        QComboBox::drop-down {
            subcontrol-origin: border;
            subcontrol-position: top right;
            width: 24px;
            border-left: 1px solid %5;
            border-top-right-radius: 5px;
            border-bottom-right-radius: 5px;
            background: transparent;
        }
        QComboBox::drop-down:hover {
            background-color: %6;
        }
        QComboBox::down-arrow {
            image: url()" + arrowDown + R"();
            width: 10px;
            height: 10px;
        }
        QComboBox QAbstractItemView {
            background-color: %8;
            border: 1px solid %9;
            selection-background-color: %10;
            selection-color: %1;
            padding: 4px;
            outline: none;
        }
        QListView, QListWidget {
            background-color: transparent;
            border: none;
            outline: none;
        }
        QListView::item, QListWidget::item {
            border-radius: 8px;
            padding: 4px;
        }
        QListView::item:hover, QListWidget::item:hover {
            background-color: transparent;
        }
        QListView::item:selected, QListWidget::item:selected {
            background-color: transparent;
        }
        QToolTip {
            background-color: %8;
            color: %1;
            border: 1px solid %9;
            border-radius: 6px;
            padding: 5px 10px;
        }
        QMenu {
            background-color: %8;
            color: %1;
            border: 1px solid %9;
            border-radius: 8px;
            padding: 4px;
        }
        QMenu::item {
            background-color: transparent;
            padding: 5px 14px 5px 10px;
            border-radius: 5px;
            color: %1;
            font-size: 13px;
        }
        QMenu::item:selected {
            background-color: %10;
            color: %7;
        }
        QMenu::item:disabled {
            color: %3;
        }
        QMenu::separator {
            height: 1px;
            background-color: %11;
            margin: 4px 6px;
        }
    )")
    .arg(text, bg, secText, cardBg, border, hover, primary, menuBg, menuBorder, menuHover, menuSep);
}
