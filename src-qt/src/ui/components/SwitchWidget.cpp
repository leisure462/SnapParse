#include "SwitchWidget.h"
#include "ThemeManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

SwitchWidget::SwitchWidget(QWidget* parent) : QWidget(parent) {
    setCursor(Qt::PointingHandCursor);
    setFixedSize(44, 22);

    m_animation = new QPropertyAnimation(this, "handlePosition", this);
    m_animation->setDuration(150);
}

void SwitchWidget::setChecked(bool checked) {
    if (m_checked == checked) return;
    m_checked = checked;

    m_animation->stop();
    m_animation->setStartValue(m_handlePosition);
    m_animation->setEndValue(checked ? 1.0 : 0.0);
    m_animation->start();

    update();
}

void SwitchWidget::setHandlePosition(qreal pos) {
    m_handlePosition = pos;
    update();
}

void SwitchWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && isEnabled()) {
        setChecked(!m_checked);
        emit toggled(m_checked);
    }
    QWidget::mouseReleaseEvent(event);
}

void SwitchWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    qreal h = height();
    qreal w = width();
    qreal radius = h / 2.0;

    // Track Background
    QColor trackColor;
    if (!isEnabled()) {
        trackColor = ThemeManager::instance()->borderColor();
    } else if (m_checked) {
        trackColor = ThemeManager::instance()->primaryColor();
    } else {
        trackColor = ThemeManager::instance()->borderColor();
    }

    QPainterPath path;
    path.addRoundedRect(0, 0, w, h, radius, radius);
    painter.fillPath(path, trackColor);

    // Handle (Circle)
    qreal handleRadius = (h - 4) / 2.0;
    qreal startX = 2.0 + handleRadius;
    qreal endX = w - 2.0 - handleRadius;
    qreal currentX = startX + m_handlePosition * (endX - startX);

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(QPointF(currentX, h / 2.0), handleRadius, handleRadius);
}
