#pragma once

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include "FluentIcon.h"

class FluentSearchLineEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit FluentSearchLineEdit(QWidget* parent = nullptr);

    void setPlaceholderText(const QString& text);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateStyles();

    QLabel* m_searchIcon = nullptr;
    QPushButton* m_clearBtn = nullptr;
};
