#include "NoteDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

NoteDialog::NoteDialog(const QString& initialNote, QWidget* parent) : QDialog(parent) {
    setWindowTitle("添加/修改备注");
    setFixedSize(340, 200);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    layout->addWidget(new QLabel("备注内容 (保存后优先展示备注):", this));

    m_textEdit = new QTextEdit(this);
    m_textEdit->setPlainText(initialNote);
    m_textEdit->setStyleSheet("border: 1px solid #e5e5ea; border-radius: 6px; padding: 6px;");
    layout->addWidget(m_textEdit, 1);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* btnCancel = new QPushButton("取消", this);
    QPushButton* btnSave = new QPushButton("保存", this);
    btnSave->setStyleSheet("QPushButton { background: #1677ff; color: #ffffff; font-weight: bold; }");
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnSave, &QPushButton::clicked, this, &QDialog::accept);

    btnLayout->addStretch();
    btnLayout->addWidget(btnCancel);
    btnLayout->addWidget(btnSave);
    layout->addLayout(btnLayout);
}
