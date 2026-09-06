#pragma once

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QButtonGroup>
#include "FluentSearchLineEdit.h"
#include "SearchResultPopup.h"

class PreferenceHeader : public QWidget {
    Q_OBJECT
public:
    explicit PreferenceHeader(QWidget* parent = nullptr);

    void setTabTitle(const QString& title);
    void setSections(const QList<QPair<QString, QString>>& sections, int activeIndex = 0);

signals:
    void sectionSelected(int index);
    void searchResultPicked(const QString& tabId, const QString& sectionId, const QString& settingId);

private slots:
    void handleSearchTextChanged(const QString& text);

private:
    QLabel* m_titleLabel = nullptr;
    FluentSearchLineEdit* m_searchEdit = nullptr;
    SearchResultPopup* m_searchPopup = nullptr;
    QHBoxLayout* m_sectionsLayout = nullptr;
    QButtonGroup* m_sectionButtons = nullptr;

    QList<QPair<QString, QString>> m_lastSections;
    int m_lastActiveIndex = 0;
};
