#pragma once

#include <QWidget>
#include <QListWidget>

struct PreferenceSearchItem {
    QString tabId;
    QString sectionId;
    QString settingId;
    QString title;
    QString description;
};

class SearchResultPopup : public QWidget {
    Q_OBJECT
public:
    explicit SearchResultPopup(QWidget* parent = nullptr);

    void search(const QString& query);

signals:
    void itemSelected(const QString& tabId, const QString& sectionId, const QString& settingId);

private:
    void initIndex();

    QListWidget* m_list = nullptr;
    QList<PreferenceSearchItem> m_allSearchItems;
};
