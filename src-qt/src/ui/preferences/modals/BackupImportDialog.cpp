#include "BackupImportDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QMessageBox>

BackupImportDialog::BackupImportDialog(const QString& filePath, QWidget* parent) 
    : QDialog(parent), m_filePath(filePath) {
    setWindowTitle("导入 SnapParse 备份");
    setFixedSize(400, 320);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    QFileInfo fi(filePath);
    QLabel* fileLabel = new QLabel(QString("备份文件: %1").arg(fi.fileName()), this);
    fileLabel->setStyleSheet("font-weight: 600; font-size: 13px;");
    layout->addWidget(fileLabel);

    QLabel* pwdLabel = new QLabel("备份密码 (若未加密可留空):", this);
    layout->addWidget(pwdLabel);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_passwordEdit);

    QLabel* stratLabel = new QLabel("导入方式:", this);
    layout->addWidget(stratLabel);

    QHBoxLayout* stratLayout = new QHBoxLayout();
    m_rbMerge = new QRadioButton("合并导入", this);
    m_rbOverwrite = new QRadioButton("覆盖导入", this);
    m_rbMerge->setChecked(true);
    stratLayout->addWidget(m_rbMerge);
    stratLayout->addWidget(m_rbOverwrite);
    layout->addLayout(stratLayout);

    connect(m_rbMerge, &QRadioButton::toggled, this, &BackupImportDialog::handleStrategyChanged);
    connect(m_rbOverwrite, &QRadioButton::toggled, this, &BackupImportDialog::handleStrategyChanged);

    m_warningLabel = new QLabel("⚠️ 覆盖导入会立即清空并替换当前所有数据，请谨慎操作。", this);
    m_warningLabel->setStyleSheet("color: #cf1322; font-size: 12px; background: rgba(255, 77, 79, 0.1); padding: 8px; border-radius: 6px;");
    m_warningLabel->setWordWrap(true);
    m_warningLabel->hide();
    layout->addWidget(m_warningLabel);

    m_overwriteConfirmCheck = new QCheckBox("我已阅读风险提示，确认覆盖当前数据", this);
    m_overwriteConfirmCheck->hide();
    layout->addWidget(m_overwriteConfirmCheck);

    layout->addStretch();

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* btnCancel = new QPushButton("取消", this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    m_btnImport = new QPushButton("导入", this);
    m_btnImport->setStyleSheet("QPushButton { background: #1677ff; color: #ffffff; font-weight: bold; }");
    connect(m_btnImport, &QPushButton::clicked, this, &BackupImportDialog::handleImport);

    btnLayout->addStretch();
    btnLayout->addWidget(btnCancel);
    btnLayout->addWidget(m_btnImport);
    layout->addLayout(btnLayout);

    connect(&m_timer, &QTimer::timeout, this, &BackupImportDialog::updateOverwriteTimer);
}

void BackupImportDialog::handleStrategyChanged() {
    if (m_rbOverwrite->isChecked()) {
        m_warningLabel->show();
        m_overwriteConfirmCheck->show();
        m_overwriteConfirmCheck->setChecked(false);
        m_overwriteConfirmCheck->setEnabled(false);

        m_countdown = 3;
        m_btnImport->setEnabled(false);
        m_btnImport->setText(QString("确认覆盖 (%1s)").arg(m_countdown));
        m_btnImport->setStyleSheet("QPushButton { background: #ff4d4f; color: #ffffff; font-weight: bold; }");
        m_timer.start(1000);
    } else {
        m_timer.stop();
        m_warningLabel->hide();
        m_overwriteConfirmCheck->hide();
        m_btnImport->setEnabled(true);
        m_btnImport->setText("导入");
        m_btnImport->setStyleSheet("QPushButton { background: #1677ff; color: #ffffff; font-weight: bold; }");
    }
}

void BackupImportDialog::updateOverwriteTimer() {
    m_countdown--;
    if (m_countdown <= 0) {
        m_timer.stop();
        m_overwriteConfirmCheck->setEnabled(true);
        m_btnImport->setText("覆盖导入");
        connect(m_overwriteConfirmCheck, &QCheckBox::toggled, m_btnImport, &QPushButton::setEnabled);
    } else {
        m_btnImport->setText(QString("确认覆盖 (%1s)").arg(m_countdown));
    }
}

void BackupImportDialog::handleImport() {
    if (m_rbOverwrite->isChecked() && !m_overwriteConfirmCheck->isChecked()) {
        QMessageBox::warning(this, "提示", "请先勾选确认覆盖当前数据");
        return;
    }
    accept();
}
