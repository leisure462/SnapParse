#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QProgressBar>
#include <QStackedWidget>

class InstallerWindow : public QWidget {
    Q_OBJECT
public:
    explicit InstallerWindow(QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void handleBrowse();
    void handleStartInstall();
    void handleFinish();

private:
    void setupUi();
    bool performInstallation(const QString& targetDir);
    bool createShortcuts(const QString& targetExe);
    void registerUninstaller(const QString& targetDir);

    QStackedWidget* m_stackedWidget = nullptr;

    // Page 1: Config
    QLineEdit* m_pathEdit = nullptr;
    QCheckBox* m_chkDesktop = nullptr;
    QCheckBox* m_chkStartMenu = nullptr;
    QCheckBox* m_chkAutoStart = nullptr;
    QPushButton* m_btnInstall = nullptr;

    // Page 2: Progress
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;

    // Page 3: Finish
    QCheckBox* m_chkLaunch = nullptr;
    QPushButton* m_btnFinish = nullptr;

    QString m_installedExePath;
    bool m_dragging = false;
    QPoint m_dragPosition;
};
