#include "FluentFontSettingCard.h"
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QPainter>

namespace {
class LazyFontItemDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        QString fam = index.data(Qt::UserRole).toString();
        if (!fam.isEmpty()) {
            opt.font.setFamily(fam);
            opt.font.setPointSize(10);
        }
        QStyledItemDelegate::paint(painter, opt, index);
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize s = QStyledItemDelegate::sizeHint(option, index);
        s.setHeight(qMax(s.height(), 28));
        return s;
    }
};
}

FluentFontSettingCard::FluentFontSettingCard(const QString& title, const QString& content, QWidget* parent)
    : FluentSettingCard(title, content, parent) {
    setupComboBox();
}

FluentFontSettingCard::FluentFontSettingCard(FluentIconType icon, const QString& title, const QString& content, QWidget* parent)
    : FluentSettingCard(icon, title, content, parent) {
    setupComboBox();
}

void FluentFontSettingCard::setupComboBox() {
    m_comboBox = new QComboBox(this);
    m_comboBox->setMinimumWidth(220);
    m_comboBox->setMaximumWidth(280);
    m_comboBox->setFixedHeight(32);
    m_comboBox->setMaxVisibleItems(16);
    m_comboBox->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    m_comboBox->setItemDelegate(new LazyFontItemDelegate(m_comboBox));

    // 1. Default system entry
    m_comboBox->addItem("跟随系统默认 (Microsoft YaHei UI)", "");

    // 2. Enumerate installed system fonts
    QStringList families = QFontDatabase::families();
    QStringList filtered;
    filtered.reserve(families.size());

    for (const QString& fam : families) {
        // Filter vertical-only fonts in Windows (starting with @)
        if (fam.startsWith('@')) continue;
        filtered.append(fam);
    }
    filtered.removeDuplicates();
    filtered.sort(Qt::CaseInsensitive);

    for (const QString& fam : filtered) {
        m_comboBox->addItem(fam, fam);
    }

    // Only fire when user explicitly activates an item from dropdown
    connect(m_comboBox, &QComboBox::activated, this, [this](int) {
        emit fontChanged(currentFont());
    });

    setTrailingWidget(m_comboBox);
}

void FluentFontSettingCard::setCurrentFont(const QString& family) {
    if (!m_comboBox) return;
    const QSignalBlocker blocker(m_comboBox);

    if (family.isEmpty() || family == "default") {
        m_comboBox->setCurrentIndex(0);
    } else {
        int idx = m_comboBox->findData(family);
        if (idx >= 0) {
            m_comboBox->setCurrentIndex(idx);
        } else {
            m_comboBox->setCurrentIndex(0);
        }
    }
}

QString FluentFontSettingCard::currentFont() const {
    if (!m_comboBox) return QString();
    return m_comboBox->currentData().toString();
}
