#include "InstallerWindow.h"
#include <QPainter>
#include <QPainterPath>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcess>
#include <QTimer>
#include <QMouseEvent>
#include <QSettings>
#include <QApplication>
#include <windows.h>
#include <shlobj.h>
#include <wrl/client.h>

static bool createWindowsShortcut(const QString& targetExe, const QString& shortcutPath, const QString& description) {
    CoInitialize(NULL);
    Microsoft::WRL::ComPtr<IShellLinkW> shellLink;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&shellLink);
    if (SUCCEEDED(hr)) {
        shellLink->SetPath((LPCWSTR)targetExe.utf16());
        shellLink->SetWorkingDirectory((LPCWSTR)QFileInfo(targetExe).absolutePath().utf16());
        if (!description.isEmpty()) {
            shellLink->SetDescription((LPCWSTR)description.utf16());
        }
        shellLink->SetIconLocation((LPCWSTR)targetExe.utf16(), 0);

        Microsoft::WRL::ComPtr<IPersistFile> persistFile;
        hr = shellLink.As(&persistFile);
        if (SUCCEEDED(hr)) {
            hr = persistFile->Save((LPCWSTR)shortcutPath.utf16(), TRUE);
            CoUninitialize();
            return SUCCEEDED(hr);
        }
    }
    CoUninitialize();
    return false;
}

InstallerWindow::InstallerWindow(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(540, 420);

    setupUi();
}

void InstallerWindow::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect r = rect().adjusted(1, 1, -1, -1);
    QPainterPath path;
    path.addRoundedRect(r, 12, 12);

    // Dark Mica style background
    painter.fillPath(path, QColor(28, 28, 34, 250));

    // Subtle border
    painter.setPen(QPen(QColor(255, 255, 255, 30), 1));
    painter.drawPath(path);
}

void InstallerWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
}

void InstallerWindow::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragPosition);
    }
}

void InstallerWindow::mouseReleaseEvent(QMouseEvent*) {
    m_dragging = false;
}

void InstallerWindow::setupUi() {
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(28, 24, 28, 24);
    rootLayout->setSpacing(16);

    // 1. Top Header Bar (App Icon + Title + Close button)
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(14);

    QLabel* iconLabel = new QLabel(this);
    iconLabel->setFixedSize(48, 48);
    QPixmap iconPix(":/icons/app_icon.svg");
    if (!iconPix.isNull()) {
        iconLabel->setPixmap(iconPix.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    headerLayout->addWidget(iconLabel);

    QVBoxLayout* titleLayout = new QVBoxLayout();
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(3);

    QLabel* titleLabel = new QLabel("SnapParse 安装向导", this);
    titleLabel->setStyleSheet("font-family: 'Microsoft YaHei UI'; font-size: 18px; font-weight: bold; color: #ffffff;");
    titleLayout->addWidget(titleLabel);

    QLabel* subtitleLabel = new QLabel("v3.0.0 极简智能剪贴板管理工具", this);
    subtitleLabel->setStyleSheet("font-family: 'Microsoft YaHei UI'; font-size: 12.5px; color: #86868b;");
    titleLayout->addWidget(subtitleLabel);

    headerLayout->addLayout(titleLayout, 1);

    QPushButton* closeBtn = new QPushButton("✕", this);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet("QPushButton { border: none; background: transparent; color: #86868b; font-size: 14px; border-radius: 4px; } QPushButton:hover { background: rgba(255, 77, 79, 0.2); color: #ff4d4f; }");
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
    headerLayout->addWidget(closeBtn, 0, Qt::AlignTop);

    rootLayout->addLayout(headerLayout);

    // 2. Stacked Pages
    m_stackedWidget = new QStackedWidget(this);

    // ==========================================
    // Page 0: Configuration Page
    // ==========================================
    QWidget* page0 = new QWidget(this);
    QVBoxLayout* page0Layout = new QVBoxLayout(page0);
    page0Layout->setContentsMargins(0, 10, 0, 0);
    page0Layout->setSpacing(14);

    QLabel* pathTitle = new QLabel("安装目标路径:", page0);
    pathTitle->setStyleSheet("font-family: 'Microsoft YaHei UI'; font-size: 13px; font-weight: 500; color: #d1d1d6;");
    page0Layout->addWidget(pathTitle);

    QHBoxLayout* pathLayout = new QHBoxLayout();
    pathLayout->setContentsMargins(0, 0, 0, 0);
    pathLayout->setSpacing(8);

    QString defaultInstallDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Programs/SnapParse";
    defaultInstallDir = QDir::toNativeSeparators(defaultInstallDir);

    m_pathEdit = new QLineEdit(defaultInstallDir, page0);
    m_pathEdit->setFixedHeight(34);
    m_pathEdit->setStyleSheet("QLineEdit { background: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255, 255, 255, 0.15); border-radius: 6px; color: #ffffff; padding: 0 10px; font-family: 'Microsoft YaHei UI'; font-size: 12.5px; }");
    pathLayout->addWidget(m_pathEdit, 1);

    QPushButton* browseBtn = new QPushButton("浏览...", page0);
    browseBtn->setFixedHeight(34);
    browseBtn->setCursor(Qt::PointingHandCursor);
    browseBtn->setStyleSheet("QPushButton { background: rgba(255, 255, 255, 0.08); border: 1px solid rgba(255, 255, 255, 0.15); border-radius: 6px; color: #ffffff; padding: 0 14px; font-family: 'Microsoft YaHei UI'; font-size: 12.5px; } QPushButton:hover { background: rgba(255, 255, 255, 0.15); }");
    connect(browseBtn, &QPushButton::clicked, this, &InstallerWindow::handleBrowse);
    pathLayout->addWidget(browseBtn);

    page0Layout->addLayout(pathLayout);

    // Options Checkboxes
    QVBoxLayout* optsLayout = new QVBoxLayout();
    optsLayout->setContentsMargins(4, 4, 4, 4);
    optsLayout->setSpacing(8);

    QString chkStyle = "QCheckBox { font-family: 'Microsoft YaHei UI'; font-size: 13px; color: #e5e5ea; spacing: 8px; } QCheckBox::indicator { width: 18px; height: 18px; border-radius: 4px; border: 1px solid rgba(255, 255, 255, 0.2); background: rgba(255, 255, 255, 0.05); } QCheckBox::indicator:checked { background: #1677ff; border-color: #1677ff; image: none; }";

    m_chkDesktop = new QCheckBox("创建桌面快捷方式", page0);
    m_chkDesktop->setChecked(true);
    m_chkDesktop->setStyleSheet(chkStyle);
    optsLayout->addWidget(m_chkDesktop);

    m_chkStartMenu = new QCheckBox("添加到开始菜单程序组", page0);
    m_chkStartMenu->setChecked(true);
    m_chkStartMenu->setStyleSheet(chkStyle);
    optsLayout->addWidget(m_chkStartMenu);

    m_chkAutoStart = new QCheckBox("开机自动启动 SnapParse", page0);
    m_chkAutoStart->setChecked(true);
    m_chkAutoStart->setStyleSheet(chkStyle);
    optsLayout->addWidget(m_chkAutoStart);

    page0Layout->addLayout(optsLayout);
    page0Layout->addStretch();

    m_btnInstall = new QPushButton("立即安装", page0);
    m_btnInstall->setFixedHeight(40);
    m_btnInstall->setCursor(Qt::PointingHandCursor);
    m_btnInstall->setStyleSheet("QPushButton { background-color: #1677ff; color: #ffffff; font-family: 'Microsoft YaHei UI'; font-size: 14px; font-weight: bold; border: none; border-radius: 8px; } QPushButton:hover { background-color: #4096ff; }");
    connect(m_btnInstall, &QPushButton::clicked, this, &InstallerWindow::handleStartInstall);
    page0Layout->addWidget(m_btnInstall);

    m_stackedWidget->addWidget(page0);

    // ==========================================
    // Page 1: Installing Progress Page
    // ==========================================
    QWidget* page1 = new QWidget(this);
    QVBoxLayout* page1Layout = new QVBoxLayout(page1);
    page1Layout->setContentsMargins(0, 40, 0, 0);
    page1Layout->setSpacing(18);

    m_statusLabel = new QLabel("正在准备安装环境...", page1);
    m_statusLabel->setStyleSheet("font-family: 'Microsoft YaHei UI'; font-size: 14px; color: #ffffff;");
    page1Layout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(page1);
    m_progressBar->setFixedHeight(12);
    m_progressBar->setTextVisible(false);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setStyleSheet("QProgressBar { background: rgba(255, 255, 255, 0.1); border-radius: 6px; border: none; } QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1677ff, stop:1 #69b1ff); border-radius: 6px; }");
    page1Layout->addWidget(m_progressBar);

    page1Layout->addStretch();
    m_stackedWidget->addWidget(page1);

    // ==========================================
    // Page 2: Finished Page
    // ==========================================
    QWidget* page2 = new QWidget(this);
    QVBoxLayout* page2Layout = new QVBoxLayout(page2);
    page2Layout->setContentsMargins(0, 20, 0, 0);
    page2Layout->setSpacing(16);

    QLabel* finishIcon = new QLabel("🎉", page2);
    finishIcon->setAlignment(Qt::AlignCenter);
    finishIcon->setStyleSheet("font-size: 42px;");
    page2Layout->addWidget(finishIcon);

    QLabel* finishTitle = new QLabel("SnapParse 安装成功！", page2);
    finishTitle->setAlignment(Qt::AlignCenter);
    finishTitle->setStyleSheet("font-family: 'Microsoft YaHei UI'; font-size: 18px; font-weight: bold; color: #ffffff;");
    page2Layout->addWidget(finishTitle);

    QLabel* finishDesc = new QLabel("按 Alt+C 唤出剪贴板，按 Alt+X 唤出偏好设置", page2);
    finishDesc->setAlignment(Qt::AlignCenter);
    finishDesc->setStyleSheet("font-family: 'Microsoft YaHei UI'; font-size: 13px; color: #86868b;");
    page2Layout->addWidget(finishDesc);

    page2Layout->addStretch();

    m_chkLaunch = new QCheckBox("立即运行 SnapParse", page2);
    m_chkLaunch->setChecked(true);
    m_chkLaunch->setStyleSheet(chkStyle);
    page2Layout->addWidget(m_chkLaunch, 0, Qt::AlignCenter);

    m_btnFinish = new QPushButton("完成", page2);
    m_btnFinish->setFixedHeight(40);
    m_btnFinish->setCursor(Qt::PointingHandCursor);
    m_btnFinish->setStyleSheet("QPushButton { background-color: #1677ff; color: #ffffff; font-family: 'Microsoft YaHei UI'; font-size: 14px; font-weight: bold; border: none; border-radius: 8px; } QPushButton:hover { background-color: #4096ff; }");
    connect(m_btnFinish, &QPushButton::clicked, this, &InstallerWindow::handleFinish);
    page2Layout->addWidget(m_btnFinish);

    m_stackedWidget->addWidget(page2);

    rootLayout->addWidget(m_stackedWidget, 1);
}

void InstallerWindow::handleBrowse() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择安装目录", m_pathEdit->text());
    if (!dir.isEmpty()) {
        m_pathEdit->setText(QDir::toNativeSeparators(dir));
    }
}

void InstallerWindow::handleStartInstall() {
    QString targetDir = m_pathEdit->text().trimmed();
    if (targetDir.isEmpty()) return;

    m_stackedWidget->setCurrentIndex(1);
    m_progressBar->setValue(10);
    m_statusLabel->setText("正在停止正在运行的 SnapParse 实例...");

    QTimer::singleShot(200, this, [this, targetDir]() {
        // Kill existing process if running
        QProcess::execute("taskkill", QStringList() << "/f" << "/im" << "SnapParse.exe");

        m_progressBar->setValue(30);
        m_statusLabel->setText("正在释放并安装程序组件...");

        QTimer::singleShot(200, this, [this, targetDir]() {
            bool ok = performInstallation(targetDir);
            if (!ok) {
                m_statusLabel->setText("安装失败，请检查写入权限或目标路径。");
                return;
            }

            m_progressBar->setValue(75);
            m_statusLabel->setText("正在创建快捷方式与注册系统项...");

            m_installedExePath = targetDir + "/SnapParse.exe";
            createShortcuts(m_installedExePath);
            registerUninstaller(targetDir);

            if (m_chkAutoStart->isChecked()) {
                QSettings runReg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
                runReg.setValue("SnapParse", QString("\"%1\" --autostart").arg(QDir::toNativeSeparators(m_installedExePath)));
            }

            m_progressBar->setValue(100);
            m_statusLabel->setText("安装完成！");

            QTimer::singleShot(400, this, [this]() {
                m_stackedWidget->setCurrentIndex(2);
            });
        });
    });
}

bool InstallerWindow::performInstallation(const QString& targetDir) {
    QDir dir(targetDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Extract payload from embedded resource
    QFile resFile(":/installer/payload.zip");
    if (!resFile.open(QIODevice::ReadOnly)) {
        return false;
    }

    QString tempZip = QDir::tempPath() + "/snapparse_payload.zip";
    QFile tmp(tempZip);
    if (tmp.open(QIODevice::WriteOnly)) {
        tmp.write(resFile.readAll());
        tmp.close();
    }
    resFile.close();

    // Use Windows 10/11 built-in tar.exe to extract zip archive
    QProcess tarProc;
    tarProc.start("tar", QStringList() << "-xf" << tempZip << "-C" << targetDir);
    tarProc.waitForFinished(15000);

    QFile::remove(tempZip);

    return QFile::exists(targetDir + "/SnapParse.exe");
}

bool InstallerWindow::createShortcuts(const QString& targetExe) {
    QString nativeExe = QDir::toNativeSeparators(targetExe);

    if (m_chkDesktop->isChecked()) {
        QString desktopDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        QString shortcutPath = desktopDir + "/SnapParse.lnk";
        createWindowsShortcut(nativeExe, shortcutPath, "SnapParse 智能剪贴板管理工具");
    }

    if (m_chkStartMenu->isChecked()) {
        QString startMenuDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
        QString shortcutPath = startMenuDir + "/SnapParse.lnk";
        createWindowsShortcut(nativeExe, shortcutPath, "SnapParse 智能剪贴板管理工具");
    }

    return true;
}

void InstallerWindow::registerUninstaller(const QString& targetDir) {
    QString nativeDir = QDir::toNativeSeparators(targetDir);
    QString nativeExe = nativeDir + "\\SnapParse.exe";
    QString uninstallerBat = nativeDir + "\\Uninstall.bat";

    // Write clean Uninstall.bat
    QFile unFile(uninstallerBat);
    if (unFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&unFile);
        ts << "@echo off\r\n";
        ts << "title SnapParse 卸载程序\r\n";
        ts << "taskkill /f /im SnapParse.exe >nul 2>&1\r\n";
        ts << "timeout /t 1 /nobreak >nul\r\n";
        ts << "del /f /q \"%USERPROFILE%\\Desktop\\SnapParse.lnk\" >nul 2>&1\r\n";
        ts << "del /f /q \"%APPDATA%\\Microsoft\\Windows\\Start Menu\\Programs\\SnapParse.lnk\" >nul 2>&1\r\n";
        ts << "reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SnapParse\" /f >nul 2>&1\r\n";
        ts << "reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\" /v \"SnapParse\" /f >nul 2>&1\r\n";
        ts << "cd /d \"%TEMP%\"\r\n";
        ts << "rd /s /q \"" << nativeDir << "\" >nul 2>&1\r\n";
        ts << "echo SnapParse 已卸载完成。\r\n";
        unFile.close();
    }

    // Register into Windows Control Panel & Settings "Installed Apps"
    QSettings unReg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SnapParse", QSettings::NativeFormat);
    unReg.setValue("DisplayName", "SnapParse");
    unReg.setValue("DisplayVersion", "3.0.0");
    unReg.setValue("Publisher", "EcoPasteHub");
    unReg.setValue("DisplayIcon", QString("\"%1\",0").arg(nativeExe));
    unReg.setValue("InstallLocation", QString("\"%1\"").arg(nativeDir));
    unReg.setValue("UninstallString", QString("\"%1\"").arg(uninstallerBat));
}

void InstallerWindow::handleFinish() {
    if (m_chkLaunch->isChecked() && QFile::exists(m_installedExePath)) {
        QProcess::startDetached(m_installedExePath, QStringList());
    }
    QApplication::quit();
}
