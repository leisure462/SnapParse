#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include "SegmentedWidget.h"

class BackupExportDialog : public QDialog {
    Q_OBJECT
public:
    explicit BackupExportDialog(QWidget* parent = nullptr);

    bool isEncrypted() const { return m_mode == "encrypted"; }
    QString password() const { return m_passwordEdit->text(); }

private slots:
    void handleModeChanged(const QString& mode);
    void handleExport();
    void updatePlainTimer();

private:
    SegmentedWidget* m_segmented = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QLineEdit* m_confirmPasswordEdit = nullptr;
    QLabel* m_passwordLabel = nullptr;
    QLabel* m_confirmLabel = nullptr;
    QLabel* m_hintLabel = nullptr;
    QCheckBox* m_plainConfirmCheck = nullptr;
    QPushButton* m_btnExport = nullptr;

    QString m_mode = "encrypted";
    int m_countdown = 3;
    QTimer m_timer;
};
