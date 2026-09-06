#pragma once

#include <QWidget>
#include <QPropertyAnimation>

class SwitchWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal handlePosition READ handlePosition WRITE setHandlePosition)

public:
    explicit SwitchWidget(QWidget* parent = nullptr);

    bool isChecked() const { return m_checked; }
    void setChecked(bool checked);

    qreal handlePosition() const { return m_handlePosition; }
    void setHandlePosition(qreal pos);

    QSize sizeHint() const override { return QSize(44, 22); }

signals:
    void toggled(bool checked);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    bool m_checked = false;
    qreal m_handlePosition = 0.0;
    QPropertyAnimation* m_animation = nullptr;
};
