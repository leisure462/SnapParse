#pragma once

#include <QDialog>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPair>
#include <QStringList>
#include <QPoint>

class SortItemCard;

class SortableTreeDialog : public QDialog {
    Q_OBJECT
public:
    explicit SortableTreeDialog(const QString& title, bool checkable = false, QWidget* parent = nullptr);

    void setItems(const QList<QPair<QString, QString>>& items, const QStringList& checkedKeys = {});
    QStringList orderedKeys() const;
    QStringList checkedKeys() const;

    void startCardDrag(SortItemCard* card, const QPoint& globalPos);
    void updateCardDrag(const QPoint& globalPos);
    void endCardDrag();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    struct SortItemData {
        QString key;
        QString label;
        bool checked;
    };

    void refreshListUi();

    bool m_checkable;
    QList<SortItemData> m_items;

    QWidget* m_cardsContainer = nullptr;
    QVBoxLayout* m_cardsLayout = nullptr;
    QList<SortItemCard*> m_cardWidgets;

    // Window drag
    bool m_draggingWindow = false;
    QPoint m_dragWindowPosition;

    // Card drag
    SortItemCard* m_activeDragCard = nullptr;
    QPoint m_dragStartGlobalPos;
};

class SortItemCard : public QFrame {
    Q_OBJECT
public:
    SortItemCard(SortableTreeDialog* dialog, const QString& key, const QString& label, bool checkable, bool checked, QWidget* parent = nullptr);

    QString key() const { return m_key; }
    QString label() const { return m_label; }
    bool isChecked() const { return m_checkbox ? m_checkbox->isChecked() : true; }
    void setChecked(bool chk) { if (m_checkbox) m_checkbox->setChecked(chk); }
    void setActiveDragging(bool active);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    SortableTreeDialog* m_dialog;
    QString m_key;
    QString m_label;
    bool m_checkable;
    QCheckBox* m_checkbox = nullptr;
    bool m_isMouseDown = false;
    QPoint m_pressPos;
};
