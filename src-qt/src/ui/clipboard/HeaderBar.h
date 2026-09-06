#pragma once

#include <QWidget>
#include <QPushButton>
#include <QButtonGroup>
#include <QLabel>
#include "FluentSearchLineEdit.h"

class HeaderBar : public QWidget {
    Q_OBJECT
public:
    explicit HeaderBar(QWidget* parent = nullptr);

    QString range() const { return m_range; }
    QString category() const { return m_category; }
    QString searchQuery() const { return m_searchEdit ? m_searchEdit->text() : ""; }
    bool isPinned() const { return m_pinBtn ? m_pinBtn->isChecked() : false; }

    void setRange(const QString& r);
    void setCategory(const QString& c);
    void clearSearch();
    void focusSearch();
    void setPinState(bool pinned);

signals:
    void filterChanged();
    void pinToggled(bool pinned);

private slots:
    void handleCategoryClicked(int id);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    void updateStyles();

    QLabel* m_appIcon = nullptr;
    FluentSearchLineEdit* m_searchEdit = nullptr;
    QPushButton* m_pinBtn = nullptr;
    QPushButton* m_settingsBtn = nullptr;

    QButtonGroup* m_categoryGroup = nullptr;
    QList<QPushButton*> m_catButtons;

    QString m_range = "all";
    QString m_category = "all";
};
