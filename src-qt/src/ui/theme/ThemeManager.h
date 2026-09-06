#pragma once

#include <QObject>
#include <QString>
#include <QColor>

class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager* instance();

    void applyTheme(const QString& theme = "auto");
    void setAccentColor(const QString& hexColor);
    void setFontFamily(const QString& family);
    QString fontFamily() const { return m_fontFamily; }
    bool isDarkMode() const { return m_isDark; }

    QColor backgroundColor() const;
    QColor cardBackgroundColor() const;
    QColor textColor() const;
    QColor secondaryTextColor() const;
    QColor primaryColor() const;
    QColor borderColor() const;
    QColor hoverColor() const;

    // Frosted Glass Material Colors
    QColor windowGlassColor() const;
    QColor cardGlassColor() const;
    QColor cardGlassHoverColor() const;
    QColor cardGlassSelectedColor() const;
    QColor glassBorderColor() const;
    QColor glassInnerGlowColor() const;

signals:
    void themeApplied();

private:
    explicit ThemeManager(QObject* parent = nullptr);

    bool detectSystemDarkMode() const;
    QString generateStyleSheet();

    bool m_isDark = false;
    QString m_fontFamily;
};
