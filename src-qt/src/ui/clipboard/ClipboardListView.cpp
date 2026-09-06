#include "ClipboardListView.h"
#include "AppConfig.h"
#include "EventBus.h"
#include "Logger.h"
#include <QKeyEvent>
#include <QMouseEvent>

ClipboardListView::ClipboardListView(QWidget* parent) : QListView(parent) {
    Logger::info("ClipboardListView constructor start");
    m_model = new QStandardItemModel(this);
    setModel(m_model);

    m_delegate = new ClipboardCardDelegate(this);
    setItemDelegate(m_delegate);

    setMouseTracking(true);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setSpacing(0);

    // Slim, sleek scrollbar with exact geometric alignment
    setStyleSheet(R"(
        QListView {
            background: transparent;
            border: none;
            outline: none;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 4px;
            margin: 4px 1px 4px 0px;
        }
        QScrollBar::handle:vertical {
            background: rgba(140, 140, 145, 0.45);
            min-height: 30px;
            border-radius: 2px;
        }
        QScrollBar::handle:vertical:hover {
            background: rgba(140, 140, 145, 0.75);
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: transparent;
        }
    )");

    m_quickBar = new QuickActionBar(this);
    m_quickBar->hide();

    connect(m_quickBar, &QuickActionBar::copyTriggered, this, &ClipboardListView::handleCopyAction);
    connect(m_quickBar, &QuickActionBar::pasteTriggered, this, &ClipboardListView::handlePasteAction);
    connect(m_quickBar, &QuickActionBar::favoriteTriggered, this, &ClipboardListView::handleFavoriteAction);
    connect(m_quickBar, &QuickActionBar::pinTriggered, this, &ClipboardListView::handlePinAction);
    connect(m_quickBar, &QuickActionBar::noteTriggered, this, &ClipboardListView::handleNoteAction);
    connect(m_quickBar, &QuickActionBar::deleteTriggered, this, &ClipboardListView::handleDeleteAction);

    connect(selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current, const QModelIndex&) {
        if (current.isValid() && current.row() < m_items.size()) {
            emit currentItemChanged(m_items[current.row()]);
        }
    });

    m_hoverFocusTimer.setSingleShot(true);
    connect(&m_hoverFocusTimer, &QTimer::timeout, this, [this]() {
        if (m_hoverFocusCandidateRow >= 0 && m_hoverFocusCandidateRow < m_items.size()) {
            QModelIndex idx = m_model->index(m_hoverFocusCandidateRow, 0);
            if (currentIndex() != idx) {
                setCurrentIndex(idx);
                selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect);
            }

            m_hoveredRow = m_hoverFocusCandidateRow;
            QRect r = visualRect(idx);
            auto item = m_items[m_hoverFocusCandidateRow];

            m_quickBar->setItemFavorite(item.isFavorite);
            m_quickBar->setItemPinned(item.isPinned);

            int barW = m_quickBar->width();
            m_quickBar->move(r.right() - barW - 10, r.top() + 7);
            m_quickBar->show();
            m_quickBar->raise();

            emit currentItemChanged(item);
        }
    });

    m_hoverPreviewTimer.setSingleShot(true);
    connect(&m_hoverPreviewTimer, &QTimer::timeout, this, [this]() {
        auto cfg = AppConfig::instance();
        if (cfg->preview.hoverEnabled && m_hoverPreviewCandidateRow >= 0 && m_hoverPreviewCandidateRow < m_items.size()) {
            emit itemPreviewRequested(m_items[m_hoverPreviewCandidateRow]);
        }
    });

    Logger::info("ClipboardListView constructor end");
}

void ClipboardListView::setItems(const QList<ClipboardItem>& items) {
    m_items = items;
    m_model->clear();
    m_quickBar->hide();
    m_hoverFocusTimer.stop();
    m_hoverFocusCandidateRow = -1;
    m_hoverPreviewTimer.stop();
    m_hoverPreviewCandidateRow = -1;

    for (int i = 0; i < items.size(); ++i) {
        const auto& it = items[i];
        QStandardItem* row = new QStandardItem();
        row->setData(it.id, Qt::UserRole);
        row->setData(it.kind, Qt::UserRole + 1);
        row->setData(it.content, Qt::UserRole + 2);
        row->setData(it.summary, Qt::UserRole + 3);
        row->setData(it.sourceAppName, Qt::UserRole + 4);
        row->setData(it.createdAt, Qt::UserRole + 5);
        row->setData(it.isFavorite, Qt::UserRole + 6);
        row->setData(it.isPinned, Qt::UserRole + 7);
        row->setData(it.note, Qt::UserRole + 8);
        row->setData(it.isSensitive, Qt::UserRole + 9);
        row->setData(it.sourceAppId, Qt::UserRole + 10);
        row->setData(it.sourceAppIcon, Qt::UserRole + 11);
        row->setData(it.subKind, Qt::UserRole + 12);
        row->setData(it.size, Qt::UserRole + 13);
        m_model->appendRow(row);
    }

    if (m_model->rowCount() > 0) {
        setCurrentIndex(m_model->index(0, 0));
    }
}

std::optional<ClipboardItem> ClipboardListView::currentItem() const {
    QModelIndex idx = currentIndex();
    if (idx.isValid() && idx.row() < m_items.size()) {
        return m_items[idx.row()];
    }
    return std::nullopt;
}

void ClipboardListView::keyPressEvent(QKeyEvent* event) {
    auto item = currentItem();
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (item) {
            emit itemSelectedForPaste(*item, AppConfig::instance()->content.pastePlain);
            return;
        }
    } else if (event->key() == Qt::Key_Space) {
        if (item && AppConfig::instance()->preview.spaceEnabled) {
            if (!event->isAutoRepeat()) {
                emit itemPreviewRequested(*item);
            }
            event->accept();
            return;
        }
    } else if (event->key() == Qt::Key_Delete) {
        if (item) {
            emit itemDeleteRequested(item->id);
            return;
        }
    } else if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
        QListView::keyPressEvent(event);
        auto it = currentItem();
        if (it) {
            QModelIndex curIdx = currentIndex();
            if (curIdx.isValid()) {
                m_hoveredRow = curIdx.row();
                QRect r = visualRect(curIdx);
                m_quickBar->setItemFavorite(it->isFavorite);
                m_quickBar->setItemPinned(it->isPinned);
                int barW = m_quickBar->width();
                m_quickBar->move(r.right() - barW - 10, r.top() + 7);
                m_quickBar->show();
                m_quickBar->raise();
            }
            emit keyboardNavigated(*it);
        }
        return;
    }

    QListView::keyPressEvent(event);
}

void ClipboardListView::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space) {
        if (AppConfig::instance()->preview.spaceEnabled) {
            if (!event->isAutoRepeat()) {
                EventBus::instance()->hidePreviewWindow();
            }
            event->accept();
            return;
        }
    }
    QListView::keyReleaseEvent(event);
}

void ClipboardListView::mousePressEvent(QMouseEvent* event) {
    m_hoverFocusTimer.stop();
    m_hoverPreviewTimer.stop();
    QListView::mousePressEvent(event);
    QModelIndex idx = indexAt(event->pos());
    if (idx.isValid() && idx.row() < m_items.size()) {
        m_hoverFocusCandidateRow = idx.row();
        m_hoveredRow = idx.row();
        if (currentIndex() != idx) {
            setCurrentIndex(idx);
            selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect);
        }

        QRect r = visualRect(idx);
        auto item = m_items[idx.row()];

        m_quickBar->setItemFavorite(item.isFavorite);
        m_quickBar->setItemPinned(item.isPinned);

        int barW = m_quickBar->width();
        m_quickBar->move(r.right() - barW - 10, r.top() + 7);
        m_quickBar->show();
        m_quickBar->raise();

        if (event->button() == Qt::MiddleButton) {
            QString midAction = AppConfig::instance()->content.middleClick;
            if (midAction == "singleClickPaste") emit itemSelectedForPaste(item, false);
            else if (midAction == "singleClickPastePlain") emit itemSelectedForPaste(item, true);
            else if (midAction == "singleClickCopy") emit itemSelectedForCopy(item, false);
            else if (midAction == "singleClickCopyPlain") emit itemSelectedForCopy(item, true);
            return;
        } else if (event->button() == Qt::LeftButton) {
            QString leftAction = AppConfig::instance()->content.autoPaste;
            if (leftAction == "singleClickPaste") {
                emit itemSelectedForPaste(item, AppConfig::instance()->content.pastePlain);
                return;
            } else if (leftAction == "singleClickCopy") {
                emit itemSelectedForCopy(item, AppConfig::instance()->content.copyPlain);
                return;
            }
        }
    }
}

void ClipboardListView::mouseDoubleClickEvent(QMouseEvent* event) {
    QModelIndex idx = indexAt(event->pos());
    if (idx.isValid() && idx.row() < m_items.size()) {
        auto item = m_items[idx.row()];
        QString leftAction = AppConfig::instance()->content.autoPaste;
        if (leftAction == "doubleClickPaste") {
            emit itemSelectedForPaste(item, AppConfig::instance()->content.pastePlain);
            return;
        } else if (leftAction == "doubleClickCopy") {
            emit itemSelectedForCopy(item, AppConfig::instance()->content.copyPlain);
            return;
        }
    }
    QListView::mouseDoubleClickEvent(event);
}

void ClipboardListView::mouseMoveEvent(QMouseEvent* event) {
    QListView::mouseMoveEvent(event);
    QModelIndex idx = indexAt(event->pos());
    if (idx.isValid() && idx.row() < m_items.size()) {
        if (m_hoverFocusCandidateRow != idx.row()) {
            m_hoverFocusCandidateRow = idx.row();
            // Start 100ms delay timer before auto-focusing on hover
            m_hoverFocusTimer.start(100);

            // Automatically close preview window when moving cursor to another item
            EventBus::instance()->hidePreviewWindow();
        }

        auto cfg = AppConfig::instance();
        if (cfg->preview.hoverEnabled) {
            if (m_hoverPreviewCandidateRow != idx.row()) {
                m_hoverPreviewCandidateRow = idx.row();
                m_hoverPreviewTimer.start(qMax(100, cfg->preview.hoverDelayMs));
            }
        } else {
            m_hoverPreviewTimer.stop();
            m_hoverPreviewCandidateRow = -1;
        }
    } else {
        m_hoverFocusTimer.stop();
        m_hoverFocusCandidateRow = -1;
        m_hoverPreviewTimer.stop();
        m_hoverPreviewCandidateRow = -1;
        m_quickBar->hide();
        EventBus::instance()->hidePreviewWindow();
    }
}

void ClipboardListView::leaveEvent(QEvent* event) {
    Q_UNUSED(event)
    m_hoverFocusTimer.stop();
    m_hoverFocusCandidateRow = -1;
    m_hoverPreviewTimer.stop();
    m_hoverPreviewCandidateRow = -1;
    m_quickBar->hide();
    EventBus::instance()->hidePreviewWindow();
}

void ClipboardListView::handleCopyAction() {
    if (m_hoveredRow >= 0 && m_hoveredRow < m_items.size()) {
        emit itemSelectedForCopy(m_items[m_hoveredRow], AppConfig::instance()->content.copyPlain);
    }
}

void ClipboardListView::handlePasteAction() {
    if (m_hoveredRow >= 0 && m_hoveredRow < m_items.size()) {
        emit itemSelectedForPaste(m_items[m_hoveredRow], AppConfig::instance()->content.pastePlain);
    }
}

void ClipboardListView::handleFavoriteAction() {
    if (m_hoveredRow >= 0 && m_hoveredRow < m_items.size()) {
        emit itemFavoriteToggled(m_items[m_hoveredRow].id);
    }
}

void ClipboardListView::handlePinAction() {
    if (m_hoveredRow >= 0 && m_hoveredRow < m_items.size()) {
        emit itemPinToggled(m_items[m_hoveredRow].id);
    }
}

void ClipboardListView::handleNoteAction() {
    if (m_hoveredRow >= 0 && m_hoveredRow < m_items.size()) {
        emit itemNoteRequested(m_items[m_hoveredRow]);
    }
}

void ClipboardListView::handleDeleteAction() {
    if (m_hoveredRow >= 0 && m_hoveredRow < m_items.size()) {
        emit itemDeleteRequested(m_items[m_hoveredRow].id);
    }
}
