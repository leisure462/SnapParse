#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include "FluentSearchLineEdit.h"
#include "SegmentedWidget.h"
#include "Models.h"

class FluentAppFilterRow;

class FluentAppFilterWidget : public QWidget {
    Q_OBJECT
public:
    explicit FluentAppFilterWidget(QWidget* parent = nullptr);

    void setAvailableApps(const QList<ClipboardApp>& apps);
    void setExcludedAppIds(const QStringList& excludedIds);
    QStringList excludedAppIds() const { return m_excludedIds; }

signals:
    void excludedAppsChanged(const QStringList& excludedIds);
    void refreshRequested();

private slots:
    void handleBrowseApp();
    void handleRefresh();
    void handleSearchChanged(const QString& text);
    void handleFilterTabChanged(const QString& tab);
    void handleAppToggled(const QString& appId, bool captureEnabled);
    void handleAppDeleted(const QString& appId);

private:
    void rebuildList();
    void updateCounters();

    FluentSearchLineEdit* m_searchEdit = nullptr;
    SegmentedWidget* m_filterSegment = nullptr;
    QPushButton* m_btnRefresh = nullptr;
    QPushButton* m_btnAddApp = nullptr;

    QWidget* m_cardContainer = nullptr;
    QVBoxLayout* m_rowsLayout = nullptr;
    QLabel* m_emptyLabel = nullptr;

    QList<ClipboardApp> m_allApps;
    QStringList m_excludedIds;
    QString m_currentFilterTab = "all"; // all, capture, ignore
    QString m_currentSearchText;
};
