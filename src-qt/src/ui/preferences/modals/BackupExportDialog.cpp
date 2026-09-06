#include "BackupExportDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>

BackupExportDialog::BackupExportDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("导出 SnapParse 备份");
    setFixedSize(380, 320);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    m_segmented = new SegmentedWidget(this);
    m_segmented->setOptions({
        {"encrypted", "加密备份（推荐）"},
        {"plain", "明文备份"}
    });
    m_segmented->setCurrentValue("encrypted");
    connect(m_segmented, &SegmentedWidget::valueChanged, this, &BackupExportDialog::handleModeChanged);
    layout->addWidget(m_segmented);

    m_hintLabel = new QLabel("备份会用你输入的密码加密。SnapParse 不会保存密码，忘记后无法恢复。", this);
    m_hintLabel->setStyleSheet("color: #86868b; font-size: 12px; background: rgba(22, 119, 255, 0.08); padding: 8px; border-radius: 6px;");
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    m_passwordLabel = new QLabel("备份密码 (至少 8 位):", this);
    layout->addWidget(m_passwordLabel);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_passwordEdit);

    m_confirmLabel = new QLabel("确认密码:", this);
    layout->addWidget(m_confirmLabel);

    m_confirmPasswordEdit = new QLineEdit(this);
    m_confirmPasswordEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_confirmPasswordEdit);

    m_plainConfirmCheck = new QCheckBox("我已阅读风险提示，确认导出未加密备份", this);
    m_plainConfirmCheck->hide();
    layout->addWidget(m_plainConfirmCheck);

    layout->addStretch();

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* btnCancel = new QPushButton("取消", this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    m_btnExport = new QPushButton("选择保存位置", this);
    m_btnExport->setStyleSheet("QPushButton { background: #1677ff; color: #ffffff; font-weight: bold; }");
    connect(m_btnExport, &QPushButton::clicked, this, &BackupExportDialog::handleExport);

    btnLayout->addStretch();
    btnLayout->addWidget(btnCancel);
    btnLayout->addWidget(m_btnExport);
    layout->addLayout(btnLayout);

    connect(&m_timer, &QTimer::timeout, this, &BackupExportDialog::updatePlainTimer);
}

void BackupExportDialog::handleModeChanged(const QString& mode) {
    m_mode = mode;
    if (mode == "encrypted") {
        m_timer.stop();
        m_hintLabel->setText("备份会用你输入的密码加密。SnapParse 不会保存密码，忘记后无法恢复。");
        m_hintLabel->setStyleSheet("color: #86868b; font-size: 12px; background: rgba(22, 119, 255, 0.08); padding: 8px; border-radius: 6px;");
        m_passwordLabel->show();
        m_passwordEdit->show();
        m_confirmLabel->show();
        m_confirmPasswordEdit->show();
        m_plainConfirmCheck->hide();
        m_btnExport->setEnabled(true);
        m_btnExport->setText("选择保存位置");
    } else {
        m_hintLabel->setText("⚠️ 警告：明文备份不受密码保护，任何拿到文件的人都可以直接读取备份内容。");
        m_hintLabel->setStyleSheet("color: #cf1322; font-size: 12px; background: rgba(255, 77, 79, 0.1); padding: 8px; border-radius: 6px;");
        m_passwordLabel->hide();
        m_passwordEdit->hide();
        m_confirmLabel->hide();
        m_confirmPasswordEdit->hide();
        m_plainConfirmCheck->show();
        m_plainConfirmCheck->setChecked(false);
        m_plainConfirmCheck->setEnabled(false);

        m_countdown = 3;
        m_btnExport->setEnabled(false);
        m_btnExport->setText(QString("阅读风险 (%1s)").arg(m_countdown));
        m_timer.start(1000);
    }
}

void BackupExportDialog::updatePlainTimer() {
    m_countdown--;
    if (m_countdown <= 0) {
        m_timer.stop();
        m_plainConfirmCheck->setEnabled(true);
        m_btnExport->setText("选择保存位置");
        connect(m_plainConfirmCheck, &QCheckBox::toggled, m_btnExport, &QPushButton::setEnabled);
    } else {
        m_btnExport->setText(QString("阅读风险 (%1s)").arg(m_countdown));
    }
}

void BackupExportDialog::handleExport() {
    if (m_mode == "encrypted") {
        QString p1 = m_passwordEdit->text();
        QString p2 = m_confirmPasswordEdit->text();
        if (p1.length() < 8) {
            QMessageBox::warning(this, "提示", "备份密码至少需要 8 个字符");
            return;
        }
        if (p1 != p2) {
            QMessageBox::warning(this, "提示", "两次输入的密码不一致");
            return;
        }
    } else {
        if (!m_plainConfirmCheck->isChecked()) {
            QMessageBox::warning(this, "提示", "请先勾选确认明文备份风险");
            return;
        }
    }
    accept();
}
