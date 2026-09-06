#include "SortableTreeDialog.h"
#include "ThemeManager.h"
#include "FluentIcon.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QApplication>
#include <algorithm>

static FluentIconType getFluentIconFor(const QString& key) {
    if (key == "files") return FluentIconType::Folder;
    if (key == "image") return FluentIconType::Image;
    if (key == "html") return FluentIconType::Globe;
    if (key == "rtf") return FluentIconType::FileText;
    if (key == "text") return FluentIconType::FileText;
    if (key == "paste") return FluentIconType::Paste;
    if (key == "pastePlain") return FluentIconType::FileText;
    if (key == "copy") return FluentIconType::Copy;
    if (key == "copyPlain") return FluentIconType::FileText;
    if (key == "star") return FluentIconType::Star;
    if (key == "pinItem") return FluentIconType::Pin;
    if (key == "note") return FluentIconType::Note;
    if (key == "delete") return FluentIconType::Trash;
    return FluentIconType::Workflow;
}

// ==========================================
// SortItemCard Implementation
// ==========================================
SortItemCard::SortItemCard(SortableTreeDialog* dialog, const QString& key, const QString& label, bool checkable, bool checked, QWidget* parent)
    : QFrame(parent), m_dialog(dialog), m_key(key), m_label(label), m_checkable(checkable) {
    setFixedSize(340, 38);
    setCursor(Qt::PointingHandCursor);

    QHBoxLayout* hLayout = new QHBoxLayout(this);
    hLayout->setContentsMargins(10, 0, 10, 0);
    hLayout->setSpacing(8);

    // 1. Drag Handle Indicator
    QLabel* dragHandle = new QLabel("⠿", this);
    dragHandle->setCursor(Qt::SizeAllCursor);
    dragHandle->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    dragHandle->setStyleSheet("background: transparent; border: none; font-size: 14px; color: rgba(255, 255, 255, 0.40); padding: 0 1px;");
    dragHandle->setToolTip("按住拖动排序");
    hLayout->addWidget(dragHandle);

    // 2. Vector SVG Icon
    QLabel* iconLabel = new QLabel(this);
    iconLabel->setFixedSize(16, 16);
    iconLabel->setScaledContents(true);
    iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    iconLabel->setPixmap(FluentIcon::make(getFluentIconFor(key), QColor("#d0d0d5"), 14).pixmap(16, 16));
    iconLabel->setStyleSheet("background: transparent; border: none;");
    hLayout->addWidget(iconLabel);

    // 3. Name Label
    QLabel* textLabel = new QLabel(label, this);
    textLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    textLabel->setStyleSheet("background: transparent; border: none; font-size: 12px; font-weight: 500; color: #f5f5f7;");
    hLayout->addWidget(textLabel, 1);

    // 4. Checkbox on the right
    if (m_checkable) {
        QString primaryHex = ThemeManager::instance()->primaryColor().name();
        m_checkbox = new QCheckBox(this);
        m_checkbox->setChecked(checked);
        m_checkbox->setCursor(Qt::PointingHandCursor);
        m_checkbox->setStyleSheet(QString(R"(
            QCheckBox {
                background: transparent;
                border: none;
            }
            QCheckBox::indicator {
                width: 15px;
                height: 15px;
                border-radius: 3px;
                border: 1.5px solid rgba(255, 255, 255, 0.35);
                background: rgba(255, 255, 255, 0.05);
            }
            QCheckBox::indicator:hover {
                border-color: %1;
            }
            QCheckBox::indicator:checked {
                background: %1;
                border-color: %1;
            }
        )").arg(primaryHex));
        hLayout->addWidget(m_checkbox, 0, Qt::AlignVCenter);
    }

    setActiveDragging(false);
}

void SortItemCard::setActiveDragging(bool active) {
    if (active) {
        QString primaryHex = ThemeManager::instance()->primaryColor().name();
        setStyleSheet(QString(R"(
            QFrame {
                background: rgba(255, 255, 255, 0.15);
                border: 1.5px solid %1;
                border-radius: 6px;
            }
        )").arg(primaryHex));
    } else {
        setStyleSheet(R"(
            QFrame {
                background: rgba(255, 255, 255, 0.05);
                border: 1px solid rgba(255, 255, 255, 0.12);
                border-radius: 6px;
            }
            QFrame:hover {
                background: rgba(255, 255, 255, 0.09);
                border-color: rgba(255, 255, 255, 0.22);
            }
        )");
    }
}

void SortItemCard::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isMouseDown = true;
        m_pressPos = event->globalPosition().toPoint();
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

void SortItemCard::mouseMoveEvent(QMouseEvent* event) {
    if (m_isMouseDown && (event->buttons() & Qt::LeftButton)) {
        if ((event->globalPosition().toPoint() - m_pressPos).manhattanLength() > 2) {
            m_dialog->startCardDrag(this, event->globalPosition().toPoint());
            m_dialog->updateCardDrag(event->globalPosition().toPoint());
        }
        event->accept();
        return;
    }
    QFrame::mouseMoveEvent(event);
}

void SortItemCard::mouseReleaseEvent(QMouseEvent* event) {
    if (m_isMouseDown) {
        m_isMouseDown = false;
        m_dialog->endCardDrag();
        event->accept();
        return;
    }
    QFrame::mouseReleaseEvent(event);
}

// ==========================================
// SortableTreeDialog Implementation
// ==========================================
SortableTreeDialog::SortableTreeDialog(const QString& title, bool checkable, QWidget* parent) 
    : QDialog(parent), m_checkable(checkable) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);

    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(8);
    rootLayout->setSizeConstraint(QLayout::SetFixedSize);

    // 1. Top Header Bar
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);

    QVBoxLayout* titleLayout = new QVBoxLayout();
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(2);

    QLabel* titleLabel = new QLabel(title, this);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #ffffff;");
    titleLayout->addWidget(titleLabel);

    QString subText = checkable 
        ? "按住左侧手柄拖动排序，勾选的项目将在快捷操作栏中显示。"
        : "按住左侧手柄拖动排序，排在前面的项目优先匹配。";
    QLabel* subLabel = new QLabel(subText, this);
    subLabel->setWordWrap(true);
    subLabel->setStyleSheet("font-size: 11px; color: #86868b;");
    titleLayout->addWidget(subLabel);

    headerLayout->addLayout(titleLayout, 1);

    QPushButton* closeBtn = new QPushButton("✕", this);
    closeBtn->setFixedSize(24, 24);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet("QPushButton { border: none; background: transparent; color: #86868b; font-size: 12px; border-radius: 4px; } QPushButton:hover { background: rgba(255, 77, 79, 0.2); color: #ff4d4f; }");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    headerLayout->addWidget(closeBtn, 0, Qt::AlignTop);

    rootLayout->addLayout(headerLayout);

    // 2. Pure Layout-based Cards Container (No QAbstractScrollArea, Zero Viewport Clipping!)
    m_cardsContainer = new QWidget(this);
    m_cardsLayout = new QVBoxLayout(m_cardsContainer);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(5);
    m_cardsLayout->setSizeConstraint(QLayout::SetFixedSize);

    rootLayout->addWidget(m_cardsContainer, 0, Qt::AlignHCenter);

    // 3. Bottom Button Bar
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(0, 2, 0, 0);
    bottomLayout->setSpacing(8);
    bottomLayout->addStretch();

    QPushButton* cancelBtn = new QPushButton("取消", this);
    cancelBtn->setFixedSize(72, 30);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setStyleSheet("QPushButton { background: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255, 255, 255, 0.15); border-radius: 6px; color: #e5e5ea; font-size: 12px; } QPushButton:hover { background: rgba(255, 255, 255, 0.15); }");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    bottomLayout->addWidget(cancelBtn);

    QString primaryHex = ThemeManager::instance()->primaryColor().name();
    QPushButton* saveBtn = new QPushButton("保存", this);
    saveBtn->setFixedSize(72, 30);
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setStyleSheet(QString("QPushButton { background-color: %1; color: #ffffff; border: none; border-radius: 6px; font-weight: bold; font-size: 12px; } QPushButton:hover { opacity: 0.90; }").arg(primaryHex));
    connect(saveBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(saveBtn);

    rootLayout->addLayout(bottomLayout);
}

void SortableTreeDialog::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect r = rect().adjusted(1, 1, -1, -1);
    QPainterPath path;
    path.addRoundedRect(r, 10, 10);

    painter.fillPath(path, QColor(26, 26, 30, 252));
    painter.setPen(QPen(QColor(255, 255, 255, 28), 1));
    painter.drawPath(path);
}

void SortableTreeDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (!m_cardsContainer->geometry().contains(event->pos())) {
            m_draggingWindow = true;
            m_dragWindowPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
            event->accept();
            return;
        }
    }
    QDialog::mousePressEvent(event);
}

void SortableTreeDialog::mouseMoveEvent(QMouseEvent* event) {
    if (m_draggingWindow && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragWindowPosition);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void SortableTreeDialog::mouseReleaseEvent(QMouseEvent* event) {
    m_draggingWindow = false;
    QDialog::mouseReleaseEvent(event);
}

void SortableTreeDialog::setItems(const QList<QPair<QString, QString>>& items, const QStringList& checkedKeys) {
    m_items.clear();
    QSet<QString> checkedSet(checkedKeys.begin(), checkedKeys.end());

    for (const auto& it : items) {
        SortItemData d;
        d.key = it.first;
        d.label = it.second;
        d.checked = checkedKeys.isEmpty() ? true : checkedSet.contains(it.first);
        m_items.append(d);
    }

    refreshListUi();
    adjustSize();
}

void SortableTreeDialog::refreshListUi() {
    // Clear old card widgets
    qDeleteAll(m_cardWidgets);
    m_cardWidgets.clear();

    for (int i = 0; i < m_items.size(); ++i) {
        const auto& d = m_items[i];
        SortItemCard* card = new SortItemCard(this, d.key, d.label, m_checkable, d.checked, m_cardsContainer);
        m_cardWidgets.append(card);
        m_cardsLayout->addWidget(card);
    }
}

void SortableTreeDialog::startCardDrag(SortItemCard* card, const QPoint& globalPos) {
    m_activeDragCard = card;
    m_dragStartGlobalPos = globalPos;
    if (m_activeDragCard) {
        m_activeDragCard->setActiveDragging(true);
    }
}

void SortableTreeDialog::updateCardDrag(const QPoint& globalPos) {
    if (!m_activeDragCard || m_cardWidgets.size() <= 1) return;

    int currentIdx = m_cardWidgets.indexOf(m_activeDragCard);
    if (currentIdx < 0) return;

    // Map globalPos to cards container local coordinate
    QPoint containerLocal = m_cardsContainer->mapFromGlobal(globalPos);
    int cardTotalH = 38 + 5; // card height + spacing
    int targetIdx = qBound(0, containerLocal.y() / cardTotalH, m_cardWidgets.size() - 1);

    if (targetIdx != currentIdx) {
        // Swap in widgets list
        m_cardWidgets.move(currentIdx, targetIdx);
        // Swap in data list
        m_items.move(currentIdx, targetIdx);

        // Relayout cards seamlessly
        m_cardsLayout->removeWidget(m_activeDragCard);
        m_cardsLayout->insertWidget(targetIdx, m_activeDragCard);
    }
}

void SortableTreeDialog::endCardDrag() {
    if (m_activeDragCard) {
        m_activeDragCard->setActiveDragging(false);
        m_activeDragCard = nullptr;
    }
}

QStringList SortableTreeDialog::orderedKeys() const {
    QStringList res;
    for (const auto* card : m_cardWidgets) {
        res.append(card->key());
    }
    return res;
}

QStringList SortableTreeDialog::checkedKeys() const {
    QStringList res;
    for (const auto* card : m_cardWidgets) {
        if (card->isChecked()) {
            res.append(card->key());
        }
    }
    return res;
}
