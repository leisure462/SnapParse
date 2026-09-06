#include "ClipboardWindow.h"
#include "AppConfig.h"
#include "DatabaseManager.h"
#include "PasteInjector.h"
#include "GlobalHookManager.h"
#include "EventBus.h"
#include "ThemeManager.h"
#include "WindowBackdropHelper.h"
#include "NoteDialog.h"
#include "Logger.h"
#include "MemoryManager.h"

#include <QPainter>
#include <QPainterPath>
#include <QGuiApplication>
#include <QScreen>
#include <QMessageBox>
#include <QKeyEvent>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

ClipboardWindow::ClipboardWindow(QWidget* parent) : QWidget(parent) {
    Logger::info("ClipboardWindow constructor start");
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);

    auto cfg = AppConfig::instance();
    int initW = qBound(280, cfg->window.windowWidth, 900);
    int initH = qBound(360, cfg->window.windowHeight, 1200);
    resize(initW, initH);
    setMinimumSize(280, 360);
    setMaximumSize(900, 1200);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 4);
    layout->setSpacing(0);

    Logger::info("Creating HeaderBar...");
    m_header = new HeaderBar(this);
    layout->addWidget(m_header);

    Logger::info("Creating ClipboardListView...");
    m_listView = new ClipboardListView(this);
    layout->addWidget(m_listView, 1);

    connect(m_header, &HeaderBar::filterChanged, this, &ClipboardWindow::handleFilterChanged);
    connect(m_header, &HeaderBar::pinToggled, this, [](bool pinned) {
        GlobalHookManager::instance()->setPinned(pinned);
    });

    connect(m_listView, &ClipboardListView::itemSelectedForPaste, this, &ClipboardWindow::handleItemPaste);
    connect(m_listView, &ClipboardListView::itemSelectedForCopy, this, &ClipboardWindow::handleItemCopy);
    connect(m_listView, &ClipboardListView::itemPreviewRequested, this, &ClipboardWindow::handleItemPreview);
    connect(m_listView, &ClipboardListView::itemFavoriteToggled, this, &ClipboardWindow::handleItemFavorite);
    connect(m_listView, &ClipboardListView::itemPinToggled, this, &ClipboardWindow::handleItemPin);
    connect(m_listView, &ClipboardListView::itemNoteRequested, this, &ClipboardWindow::handleItemNote);
    connect(m_listView, &ClipboardListView::itemDeleteRequested, this, &ClipboardWindow::handleItemDelete);
    connect(m_listView, &ClipboardListView::keyboardNavigated, this, [this](const ClipboardItem& item) {
        if (m_previewVisible) {
            handleItemPreview(item);
        }
    });

    connect(EventBus::instance(), &EventBus::previewVisibilityChanged, this, [this](bool visible) {
        m_previewVisible = visible;
    });

    connect(EventBus::instance(), &EventBus::clipboardUpdated, this, &ClipboardWindow::reloadData);
    connect(ThemeManager::instance(), &ThemeManager::themeApplied, this, [this]() {
        update();
        if (m_listView && m_listView->viewport()) {
            m_listView->viewport()->update();
        }
    });

    Logger::info("ClipboardWindow constructor end");
}

void ClipboardWindow::showAndFocus() {
    Logger::info("showAndFocus called");
    PasteInjector::instance()->recordTargetWindow();
    calculatePosition();

    auto cfg = AppConfig::instance();
    if (cfg->search.clearOnHide) {
        m_header->clearSearch();
    }

    if (cfg->window.selectRangeOnOpen != "preserve") {
        m_header->setRange(cfg->window.selectRangeOnOpen);
    }
    if (cfg->window.selectCategoryOnOpen != "preserve") {
        m_header->setCategory(cfg->window.selectCategoryOnOpen);
    }

    reloadData();

    setWindowOpacity(0.0);
    show();
    raise();
    activateWindow();

    QPropertyAnimation* anim = new QPropertyAnimation(this, "windowOpacity", this);
    anim->setDuration(120);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);

    HWND hwnd = reinterpret_cast<HWND>(winId());
    bool dark = ThemeManager::instance()->isDarkMode();
    WindowBackdropHelper::enableAcrylic(hwnd, dark);

    SetForegroundWindow(hwnd);
    GlobalHookManager::instance()->setClipboardWindowHandle(hwnd);
    GlobalHookManager::markWindowShown();

    if (cfg->window.scrollToTopOnOpen) {
        m_listView->scrollToTop();
    }

    if (cfg->search.defaultFocus) {
        m_header->focusSearch();
    } else {
        m_listView->setFocus();
    }
}

void ClipboardWindow::hideWindow() {
    auto cfg = AppConfig::instance();
    if (cfg->search.clearOnHide) {
        m_header->clearSearch();
    }
    EventBus::instance()->hidePreviewWindow();

    if (!isVisible()) return;

    QPropertyAnimation* anim = new QPropertyAnimation(this, "windowOpacity", this);
    anim->setDuration(90);
    anim->setStartValue(windowOpacity());
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::InQuad);
    connect(anim, &QPropertyAnimation::finished, this, [this]() {
        hide();
        setWindowOpacity(1.0);
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void ClipboardWindow::calculatePosition() {
    auto cfg = AppConfig::instance();
    QString mode = cfg->window.position;

    QPoint cursorPos = QCursor::pos();
    QScreen* screen = QGuiApplication::screenAt(cursorPos);
    if (!screen) screen = QGuiApplication::primaryScreen();

    QRect screenGeo = screen->availableGeometry();
    int x = cursorPos.x();
    int y = cursorPos.y();

    if (mode == "center") {
        x = screenGeo.center().x() - width() / 2;
        y = screenGeo.center().y() - height() / 2;
    } else if (mode == "remember" && cfg->window.lastWindowX >= 0 && cfg->window.lastWindowY >= 0) {
        x = cfg->window.lastWindowX;
        y = cfg->window.lastWindowY;
    } else {
        // followCursor
        x = cursorPos.x() - width() / 2;
        y = cursorPos.y() + 10;
    }

    // Keep on screen
    if (x + width() > screenGeo.right()) x = screenGeo.right() - width() - 10;
    if (x < screenGeo.left()) x = screenGeo.left() + 10;
    if (y + height() > screenGeo.bottom()) y = cursorPos.y() - height() - 10;
    if (y < screenGeo.top()) y = screenGeo.top() + 10;

    move(x, y);

    cfg->window.lastWindowX = x;
    cfg->window.lastWindowY = y;
}

void ClipboardWindow::reloadData() {
    QString range = m_header->range();
    QString category = m_header->category();
    QString search = m_header->searchQuery();
    QString group = "all";
    QString sort = AppConfig::instance()->content.sort;

    auto items = DatabaseManager::instance()->queryItems(range, category, group, search, sort, 100, 0);
    m_listView->setItems(items);
}

void ClipboardWindow::handleFilterChanged() {
    reloadData();
}

void ClipboardWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    MemoryManager::instance()->notifyWindowShown();
}

void ClipboardWindow::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    MemoryManager::instance()->notifyWindowHidden();
}

void ClipboardWindow::handleItemPaste(const ClipboardItem& item, bool plain) {
    if (AppConfig::instance()->content.updateOnReuse) {
        DatabaseManager::instance()->touchItem(item.id);
    }
    PasteInjector::instance()->pasteItem(item, plain, AppConfig::instance()->content.pasteFilesAsPath);
}

void ClipboardWindow::handleItemCopy(const ClipboardItem& item, bool plain) {
    if (AppConfig::instance()->content.updateOnReuse) {
        DatabaseManager::instance()->touchItem(item.id);
    }
    PasteInjector::instance()->writeToClipboard(item, plain, AppConfig::instance()->content.pasteFilesAsPath);
    if (AppConfig::instance()->content.copyThenHideWindow) {
        hideWindow();
    }
}

void ClipboardWindow::handleItemPreview(const ClipboardItem& item) {
    QRect anchorRect;
    if (m_listView) {
        QModelIndex idx = m_listView->currentIndex();
        if (idx.isValid()) {
            QRect r = m_listView->visualRect(idx);
            QPoint globalTopLeft = m_listView->viewport()->mapToGlobal(r.topLeft());
            anchorRect = QRect(globalTopLeft, r.size());
        }
    }
    QRect winRect = geometry();
    EventBus::instance()->showPreviewWindow(item.id, anchorRect, winRect);
}

void ClipboardWindow::handleItemFavorite(const QString& id) {
    DatabaseManager::instance()->toggleFavorite(id);
    reloadData();
}

void ClipboardWindow::handleItemPin(const QString& id) {
    DatabaseManager::instance()->togglePin(id);
    reloadData();
}

void ClipboardWindow::handleItemNote(const ClipboardItem& item) {
    NoteDialog dlg(item.note, this);
    if (dlg.exec() == QDialog::Accepted) {
        QString noteText = dlg.note().trimmed();
        DatabaseManager::instance()->updateNote(item.id, noteText);
        if (AppConfig::instance()->content.autoFavorite && !noteText.isEmpty()) {
            if (!item.isFavorite) {
                DatabaseManager::instance()->toggleFavorite(item.id);
            }
        }
        reloadData();
    }
}

void ClipboardWindow::handleItemDelete(const QString& id) {
    auto opt = DatabaseManager::instance()->getItem(id);
    if (!opt) return;

    auto item = *opt;
    auto cfg = AppConfig::instance();

    if (item.isPinned && !cfg->content.deletePinnedItems) return;
    if (item.isFavorite && !cfg->content.deleteFavoriteItems) return;

    bool needConfirm = false;
    if (item.isPinned) needConfirm = cfg->content.deletePinnedConfirm;
    else if (item.isFavorite) needConfirm = cfg->content.deleteFavoriteConfirm;
    else needConfirm = cfg->content.deleteConfirm;

    if (needConfirm) {
        if (QMessageBox::question(this, "删除确认", "确定删除这条历史记录吗？") != QMessageBox::Yes) {
            return;
        }
    }

    DatabaseManager::instance()->deleteItem(id);
    reloadData();
}

void ClipboardWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        hideWindow();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ClipboardWindow::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect r = rect().adjusted(0, 0, -1, -1);
    QPainterPath path;
    path.addRoundedRect(r, 12, 12);

    // 1. Frosted Glass Translucent Tint
    painter.fillPath(path, ThemeManager::instance()->windowGlassColor());

    // 2. Subtle Dual Border: Outer Rim + Inner Glow
    painter.setPen(QPen(ThemeManager::instance()->glassBorderColor(), 1));
    painter.drawPath(path);

    QPainterPath innerPath;
    innerPath.addRoundedRect(r.adjusted(1, 1, -1, -1), 11, 11);
    painter.setPen(QPen(ThemeManager::instance()->glassInnerGlowColor(), 1));
    painter.drawPath(innerPath);
}

int ClipboardWindow::calculateHitTestCode(const QPoint& localPos) const {
    if (localPos.x() < 0 || localPos.x() >= width() || localPos.y() < 0 || localPos.y() >= height()) {
        return HTCLIENT;
    }

    // Protect the vertical scrollbar area (rightmost ~14px below header bar) from window resize interception
    if (localPos.x() >= width() - 14 && localPos.y() >= 76) {
        return HTCLIENT;
    }

    const int MARGIN = 4;
    bool left = localPos.x() <= MARGIN;
    bool right = localPos.x() >= width() - MARGIN;
    bool top = localPos.y() <= MARGIN;
    bool bottom = localPos.y() >= height() - MARGIN;

    if (top && left) return HTTOPLEFT;
    if (top && right) return HTTOPRIGHT;
    if (bottom && left) return HTBOTTOMLEFT;
    if (bottom && right) return HTBOTTOMRIGHT;
    if (left) return HTLEFT;
    if (right) return HTRIGHT;
    if (top) return HTTOP;
    if (bottom) return HTBOTTOM;
    return HTCLIENT;
}

bool ClipboardWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_NCHITTEST) {
        QPoint localPos = mapFromGlobal(QCursor::pos());
        int ht = calculateHitTestCode(localPos);
        if (ht != HTCLIENT) {
            *result = ht;
            return true;
        }
    } else if (msg->message == WM_EXITSIZEMOVE) {
        auto cfg = AppConfig::instance();
        cfg->window.windowWidth = width();
        cfg->window.windowHeight = height();
        cfg->window.lastWindowX = x();
        cfg->window.lastWindowY = y();
        cfg->save();
    }
    return QWidget::nativeEvent(eventType, message, result);
}

void ClipboardWindow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    auto cfg = AppConfig::instance();
    cfg->window.windowWidth = width();
    cfg->window.windowHeight = height();
}
