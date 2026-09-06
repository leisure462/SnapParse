#pragma once

#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include "StorageMeterWidget.h"

class PreferenceSidebar : public QWidget {
    Q_OBJECT
public:
    explicit PreferenceSidebar(QWidget* parent = nullptr);

    void setCurrentTab(int index);
    void updateStorageUsage();

protected:
    void paintEvent(QPaintEvent* event) override;

signals:
    void tabChanged(int index);

private slots:
    void handleItemClicked(QListWidgetItem* item);

private:
    QListWidget* m_navList = nullptr;
    StorageMeterWidget* m_storageMeter = nullptr;
};
