#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include "HeaderBar.h"
#include "ClipboardListView.h"

class ClipboardWindow : public QWidget {
    Q_OBJECT
public:
    explicit ClipboardWindow(QWidget* parent = nullptr);

    void showAndFocus();
    void hideWindow();
    void reloadData();

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void handleFilterChanged();
    void handleItemPaste(const ClipboardItem& item, bool plain);
    void handleItemCopy(const ClipboardItem& item, bool plain);
    void handleItemPreview(const ClipboardItem& item);
    void handleItemFavorite(const QString& id);
    void handleItemPin(const QString& id);
    void handleItemNote(const ClipboardItem& item);
    void handleItemDelete(const QString& id);

private:
    void calculatePosition();
    int calculateHitTestCode(const QPoint& localPos) const;

    HeaderBar* m_header = nullptr;
    ClipboardListView* m_listView = nullptr;
    bool m_previewVisible = false;
};
