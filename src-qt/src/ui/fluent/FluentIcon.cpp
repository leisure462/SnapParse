#include "FluentIcon.h"
#include "ThemeManager.h"
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QSvgRenderer>
#include <QFile>
#include <QCache>
#include <cmath>

static QCache<quint64, QIcon> s_iconCache(120);

QIcon FluentIcon::make(FluentIconType type, const QColor& color, int size) {
    quint64 cacheKey = (static_cast<quint64>(type) << 40) | (static_cast<quint64>(size & 0xFF) << 32) | static_cast<quint64>(color.rgba());
    if (auto* cached = s_iconCache.object(cacheKey)) {
        return *cached;
    }

    int actualSize = size * 2; // High-DPI 2x
    QPixmap pix(actualSize, actualSize);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    double s = actualSize / 24.0; // scale from 24x24 base grid
    p.scale(s, s);

    switch (type) {
        case FluentIconType::Record:
            // Microphone / Record circle
            p.drawRoundedRect(9, 3, 6, 11, 3, 3);
            p.drawArc(QRectF(5, 7, 14, 10), 0, -180 * 16);
            p.drawLine(12, 17, 12, 21);
            p.drawLine(8, 21, 16, 21);
            break;

        case FluentIconType::History:
            // Clock
            p.drawEllipse(QPointF(12, 12), 9, 9);
            p.drawLine(12, 6, 12, 12);
            p.drawLine(12, 12, 16, 14);
            break;

        case FluentIconType::Reuse:
            // Paste / Clipboard
            p.drawRoundedRect(6, 6, 12, 15, 2, 2);
            p.drawRoundedRect(9, 3, 6, 4, 1, 1);
            p.drawLine(9, 11, 15, 11);
            p.drawLine(9, 15, 15, 15);
            break;

        case FluentIconType::Workflow:
            // App Window Desktop
            p.drawRoundedRect(3, 4, 18, 16, 2, 2);
            p.drawLine(3, 9, 21, 9);
            p.drawPoint(6, 6.5);
            p.drawPoint(9, 6.5);
            break;

        case FluentIconType::Shortcuts:
            // Keyboard
            p.drawRoundedRect(3, 6, 18, 12, 2, 2);
            p.drawLine(6, 10, 8, 10);
            p.drawLine(11, 10, 13, 10);
            p.drawLine(16, 10, 18, 10);
            p.drawLine(8, 14, 16, 14);
            break;

        case FluentIconType::Data:
            // Database / Cylinder
            p.drawEllipse(QRectF(4, 3, 16, 6));
            p.drawLine(4, 6, 4, 18);
            p.drawLine(20, 6, 20, 18);
            p.drawArc(QRectF(4, 15, 16, 6), 0, -180 * 16);
            p.drawArc(QRectF(4, 9, 16, 6), 0, -180 * 16);
            break;

        case FluentIconType::About:
            // Info circle
            p.drawEllipse(QPointF(12, 12), 9, 9);
            p.drawPoint(12, 7.5);
            p.drawLine(12, 11, 12, 16.5);
            break;

        case FluentIconType::Search:
            // Magnifying Glass
            p.drawEllipse(QPointF(10, 10), 6, 6);
            p.drawLine(14.5, 14.5, 20, 20);
            break;

        case FluentIconType::Pin:
            // Thumbtack
            p.drawRoundedRect(10, 4, 4, 6, 1, 1);
            p.drawLine(7, 10, 17, 10);
            p.drawLine(12, 10, 12, 19);
            break;

        case FluentIconType::Settings:
            // Gear
            p.drawEllipse(QPointF(12, 12), 5.5, 5.5);
            p.drawEllipse(QPointF(12, 12), 2, 2);
            for (int i = 0; i < 8; ++i) {
                double a = i * 3.14159265 / 4.0;
                p.drawLine(QPointF(12 + 5.5 * std::cos(a), 12 + 5.5 * std::sin(a)),
                           QPointF(12 + 8.5 * std::cos(a), 12 + 8.5 * std::sin(a)));
            }
            break;

        case FluentIconType::Star:
            // Star
            {
                QPainterPath starPath;
                starPath.moveTo(12, 3);
                starPath.lineTo(14.5, 8.5);
                starPath.lineTo(20.5, 9.2);
                starPath.lineTo(16, 13.3);
                starPath.lineTo(17.3, 19.2);
                starPath.lineTo(12, 16.2);
                starPath.lineTo(6.7, 19.2);
                starPath.lineTo(8, 13.3);
                starPath.lineTo(3.5, 9.2);
                starPath.lineTo(9.5, 8.5);
                starPath.closeSubpath();
                p.drawPath(starPath);
            }
            break;

        case FluentIconType::Note:
            // Pencil / Edit
            p.drawLine(4, 20, 8, 19.5);
            p.drawLine(8, 19.5, 19, 8.5);
            p.drawLine(19, 8.5, 15.5, 5);
            p.drawLine(15.5, 5, 4.5, 16);
            p.drawLine(4.5, 16, 4, 20);
            break;

        case FluentIconType::Trash:
            // Delete Bin
            p.drawLine(4, 6, 20, 6);
            p.drawRoundedRect(6, 6, 12, 15, 2, 2);
            p.drawLine(10, 10, 10, 17);
            p.drawLine(14, 10, 14, 17);
            p.drawRoundedRect(9, 3, 6, 3, 1, 1);
            break;

        case FluentIconType::Copy:
            // Copy double rect
            p.drawRoundedRect(4, 4, 11, 11, 2, 2);
            p.drawRoundedRect(9, 9, 11, 11, 2, 2);
            break;

        case FluentIconType::Paste:
            // Paste arrow
            p.drawRoundedRect(5, 5, 14, 15, 2, 2);
            p.drawLine(12, 9, 12, 16);
            p.drawLine(9, 13, 12, 16);
            p.drawLine(15, 13, 12, 16);
            break;

        case FluentIconType::Folder:
            // Folder
            {
                QPainterPath fPath;
                fPath.moveTo(3, 6);
                fPath.lineTo(9, 6);
                fPath.lineTo(11, 8);
                fPath.lineTo(21, 8);
                fPath.lineTo(21, 19);
                fPath.lineTo(3, 19);
                fPath.closeSubpath();
                p.drawPath(fPath);
            }
            break;

        case FluentIconType::Globe:
            // Globe / Link
            p.drawEllipse(QPointF(12, 12), 9, 9);
            p.drawEllipse(QPointF(12, 12), 4.5, 9);
            p.drawLine(3, 12, 21, 12);
            break;

        case FluentIconType::FileText:
            // File
            {
                QPainterPath doc;
                doc.moveTo(5, 3);
                doc.lineTo(14, 3);
                doc.lineTo(19, 8);
                doc.lineTo(19, 21);
                doc.lineTo(5, 21);
                doc.closeSubpath();
                p.drawPath(doc);
                p.drawLine(8, 12, 16, 12);
                p.drawLine(8, 16, 16, 16);
            }
            break;

        case FluentIconType::Image:
            // Image frame
            p.drawRoundedRect(3, 4, 18, 16, 2, 2);
            p.drawEllipse(QPointF(8, 9), 2, 2);
            p.drawLine(4, 17, 9, 12);
            p.drawLine(9, 12, 14, 16);
            p.drawLine(14, 16, 17, 13);
            p.drawLine(17, 13, 20, 16);
            break;

        case FluentIconType::Shield:
            // Privacy Shield
            {
                QPainterPath sPath;
                sPath.moveTo(12, 3);
                sPath.lineTo(20, 6);
                sPath.lineTo(20, 12);
                sPath.quadTo(20, 19, 12, 21);
                sPath.quadTo(4, 19, 4, 12);
                sPath.lineTo(4, 6);
                sPath.closeSubpath();
                p.drawPath(sPath);
            }
            break;

        case FluentIconType::Filter:
            // Funnel Filter
            {
                QPainterPath funnel;
                funnel.moveTo(3, 5);
                funnel.lineTo(21, 5);
                funnel.lineTo(14, 12);
                funnel.lineTo(14, 18);
                funnel.lineTo(10, 20);
                funnel.lineTo(10, 12);
                funnel.closeSubpath();
                p.drawPath(funnel);
            }
            break;

        case FluentIconType::Close:
            // Cross ✕
            p.drawLine(6, 6, 18, 18);
            p.drawLine(18, 6, 6, 18);
            break;
    }
    
    QIcon icon(pix);
    s_iconCache.insert(cacheKey, new QIcon(icon));
    return icon;
}

QIcon FluentIcon::appIcon(int size, bool filled) {
    Q_UNUSED(filled)
    QPixmap logo(":/icons/app_logo.png");
    if (!logo.isNull()) {
        int actualSize = size * 2;
        QPixmap scaled = logo.scaled(actualSize, actualSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        return QIcon(scaled);
    }
    return QIcon(":/icons/app_icon.ico");
}

QIcon FluentIcon::trayIcon(int size) {
    int actualSize = size * 2;
    QPixmap pix(actualSize, actualSize);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // Keep the default icon design style, rendered in clean neutral gray (#98989f)
    QFile svgFile(":/icons/app_icon.svg");
    if (svgFile.open(QIODevice::ReadOnly)) {
        QByteArray svgData = svgFile.readAll();
        svgData.replace("#1677ff", "#98989f");
        QSvgRenderer renderer(svgData);
        renderer.render(&p, QRect(0, 0, actualSize, actualSize));
    } else {
        return FluentIcon::make(FluentIconType::Reuse, QColor("#98989f"), size);
    }
    p.end();

    return QIcon(pix);
}
