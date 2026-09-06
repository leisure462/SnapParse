#pragma once

#include <QListView>
#include <QStandardItemModel>
#include <QTimer>
#include "Models.h"
#include "QuickActionBar.h"
#include "ClipboardCardDelegate.h"

class ClipboardListView : public QListView {
    Q_OBJECT
public:
    explicit ClipboardListView(QWidget* parent = nullptr);

    void setItems(const QList<ClipboardItem>& items);
    std::optional<ClipboardItem> currentItem() const;

signals:
    void itemSelectedForPaste(const ClipboardItem& item, bool plain = false);
    void itemSelectedForCopy(const ClipboardItem& item, bool plain = false);
    void itemPreviewRequested(const ClipboardItem& item);
    void itemFavoriteToggled(const QString& id);
    void itemPinToggled(const QString& id);
    void itemNoteRequested(const ClipboardItem& item);
    void itemDeleteRequested(const QString& id);
    void currentItemChanged(const ClipboardItem& item);
    void keyboardNavigated(const ClipboardItem& item);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void handleCopyAction();
    void handlePasteAction();
    void handleFavoriteAction();
    void handlePinAction();
    void handleNoteAction();
    void handleDeleteAction();

private:
    QStandardItemModel* m_model = nullptr;
    ClipboardCardDelegate* m_delegate = nullptr;
    QuickActionBar* m_quickBar = nullptr;
    QList<ClipboardItem> m_items;
    int m_hoveredRow = -1;
    QTimer m_hoverFocusTimer;
    int m_hoverFocusCandidateRow = -1;
    QTimer m_hoverPreviewTimer;
    int m_hoverPreviewCandidateRow = -1;
};
