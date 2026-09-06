#pragma once

#include <QWidget>
#include <QStackedWidget>
#include "PreferenceSidebar.h"
#include "PreferenceHeader.h"
#include "pages/RecordPage.h"
#include "pages/OrganizePage.h"
#include "pages/ReusePage.h"
#include "pages/WorkflowPage.h"
#include "pages/ShortcutsPage.h"
#include "pages/DataPage.h"
#include "pages/AboutPage.h"

class PreferencesWindow : public QWidget {
    Q_OBJECT
public:
    explicit PreferencesWindow(QWidget* parent = nullptr);

    void showAndFocus(const QString& targetTab = "", const QString& targetSettingId = "");

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void handleTabChanged(int index);
    void handleSectionSelected(int index);
    void handleSearchResultPicked(const QString& tabId, const QString& sectionId, const QString& settingId);

private:
    void updateHeaderForTab(int index);

    PreferenceSidebar* m_sidebar = nullptr;
    PreferenceHeader* m_header = nullptr;
    QStackedWidget* m_stack = nullptr;

    RecordPage* m_pageRecord = nullptr;
    OrganizePage* m_pageOrganize = nullptr;
    ReusePage* m_pageReuse = nullptr;
    WorkflowPage* m_pageWorkflow = nullptr;
    ShortcutsPage* m_pageShortcuts = nullptr;
    DataPage* m_pageData = nullptr;
    AboutPage* m_pageAbout = nullptr;
};
