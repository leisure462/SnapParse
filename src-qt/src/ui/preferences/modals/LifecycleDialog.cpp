#include "LifecycleDialog.h"
#include <QVBoxLayout>
#include <QHeaderView>

LifecycleDialog::LifecycleDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("窗口生命周期诊断");
    setFixedSize(520, 260);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);

    QTableWidget* table = new QTableWidget(this);
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({"窗口", "当前阶段", "保护租约", "状态"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    struct WinRow { QString name; QString phase; QString locks; QString status; };
    QList<WinRow> rows = {
        {"剪贴板窗口", "Visible / Active", "0/1", "正常运行"},
        {"偏好设置窗口", "Visible / Active", "0/0", "正常运行"},
        {"浮动预览窗口", "Dormant / Hidden", "0/0", "待机中"},
        {"系统托盘菜单", "Ready", "0/0", "已就绪"}
    };

    table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        table->setItem(i, 0, new QTableWidgetItem(rows[i].name));
        table->setItem(i, 1, new QTableWidgetItem(rows[i].phase));
        table->setItem(i, 2, new QTableWidgetItem(rows[i].locks));
        table->setItem(i, 3, new QTableWidgetItem(rows[i].status));
    }

    layout->addWidget(table);
}
