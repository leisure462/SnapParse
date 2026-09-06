#pragma once

#include <QStyledItemDelegate>
#include "Models.h"

class ClipboardCardDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ClipboardCardDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    static void clearThumbnailCache();
};
