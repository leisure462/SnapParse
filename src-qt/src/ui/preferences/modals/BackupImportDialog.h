#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QRadioButton>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QTimer>

class BackupImportDialog : public QDialog {
    Q_OBJECT
public:
    explicit BackupImportDialog(const QString& filePath, QWidget* parent = nullptr);

    QString strategy() const { return m_rbMerge->isChecked() ? "merge" : "overwrite"; }
    QString password() const { return m_passwordEdit->text(); }

private slots:
    void handleStrategyChanged();
    void handleImport();
    void updateOverwriteTimer();

private:
    QString m_filePath;
    QLineEdit* m_passwordEdit = nullptr;
    QRadioButton* m_rbMerge = nullptr;
    QRadioButton* m_rbOverwrite = nullptr;
    QLabel* m_warningLabel = nullptr;
    QCheckBox* m_overwriteConfirmCheck = nullptr;
    QPushButton* m_btnImport = nullptr;

    int m_countdown = 3;
    QTimer m_timer;
};
