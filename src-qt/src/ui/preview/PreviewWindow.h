#pragma once

#include <QWidget>
#include <QLabel>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRect>
#include "Models.h"

class PreviewWindow : public QWidget {
    Q_OBJECT
public:
    explicit PreviewWindow(QWidget* parent = nullptr);

    void showPreview(const ClipboardItem& item, const QRect& anchorRect = QRect(), const QRect& parentWindowRect = QRect(), bool isToggle = false);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void updateStyles();

    QLabel* m_titleLabel = nullptr;
    QLabel* m_metaLabel = nullptr;
    QLabel* m_badgeLabel = nullptr;
    QLabel* m_imagePreview = nullptr;
    QTextBrowser* m_textBrowser = nullptr;
    QString m_currentShowingId;
};
