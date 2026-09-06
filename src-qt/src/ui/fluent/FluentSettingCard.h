#pragma once

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "FluentIcon.h"

class FluentSettingCard : public QWidget {
    Q_OBJECT
public:
    explicit FluentSettingCard(const QString& title, const QString& content = "", QWidget* parent = nullptr);
    explicit FluentSettingCard(FluentIconType icon, const QString& title, const QString& content = "", QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setContent(const QString& content);
    void setIcon(FluentIconType icon);
    void setTrailingWidget(QWidget* widget);
    void setCardEnabled(bool enabled);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void initUi(const QString& title, const QString& content);

    QLabel* m_iconLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_contentLabel = nullptr;
    QHBoxLayout* m_mainLayout = nullptr;
    QWidget* m_trailingWidget = nullptr;
    FluentIconType m_iconType = FluentIconType::Record;
    bool m_hasIcon = false;
    bool m_isHovered = false;
};
