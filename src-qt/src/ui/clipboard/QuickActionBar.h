#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QPushButton>

class QuickActionBar : public QWidget {
    Q_OBJECT
public:
    explicit QuickActionBar(QWidget* parent = nullptr);

    void setItemFavorite(bool fav);
    void setItemPinned(bool pin);

signals:
    void copyTriggered();
    void pasteTriggered();
    void favoriteTriggered();
    void pinTriggered();
    void noteTriggered();
    void deleteTriggered();

private:
    void updateIconsAndStyles();

    QPushButton* m_btnCopy = nullptr;
    QPushButton* m_btnPaste = nullptr;
    QPushButton* m_btnStar = nullptr;
    QPushButton* m_btnPin = nullptr;
    QPushButton* m_btnNote = nullptr;
    QPushButton* m_btnDelete = nullptr;

    bool m_isFav = false;
    bool m_isPin = false;
};
