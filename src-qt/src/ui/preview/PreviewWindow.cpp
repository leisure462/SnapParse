#include "PreviewWindow.h"
#include "AppPaths.h"
#include "ThemeManager.h"
#include "WindowBackdropHelper.h"
#include "MemoryManager.h"
#include "EventBus.h"
#include <QPainter>
#include <QPainterPath>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QFileInfo>
#include <QFontMetrics>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <windows.h>

PreviewWindow::PreviewWindow(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 14, 16, 14);
    rootLayout->setSpacing(8);

    // 1. Header Bar with Title, Subtitle, and Type Badge
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(10);

    QVBoxLayout* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 13.5px; color: #ffffff;");
    textLayout->addWidget(m_titleLabel);

    m_metaLabel = new QLabel(this);
    m_metaLabel->setStyleSheet("color: #86868b; font-size: 11px;");
    textLayout->addWidget(m_metaLabel);

    headerLayout->addLayout(textLayout, 1);

    m_badgeLabel = new QLabel(this);
    m_badgeLabel->setFixedHeight(22);
    m_badgeLabel->setAlignment(Qt::AlignCenter);
    m_badgeLabel->setStyleSheet(R"(
        QLabel {
            padding: 0 8px;
            font-size: 11px;
            font-weight: bold;
            color: #ffffff;
            border-radius: 4px;
            background: rgba(255, 255, 255, 0.12);
        }
    )");
    headerLayout->addWidget(m_badgeLabel, 0, Qt::AlignTop);

    rootLayout->addLayout(headerLayout);

    // 2. Content Area
    m_imagePreview = new QLabel(this);
    m_imagePreview->setAlignment(Qt::AlignCenter);
    m_imagePreview->hide();
    rootLayout->addWidget(m_imagePreview, 1);

    m_textBrowser = new QTextBrowser(this);
    m_textBrowser->setOpenExternalLinks(true);
    rootLayout->addWidget(m_textBrowser, 1);

    connect(ThemeManager::instance(), &ThemeManager::themeApplied, this, &PreviewWindow::updateStyles);
    updateStyles();
}

void PreviewWindow::updateStyles() {
    bool dark = ThemeManager::instance()->isDarkMode();
    QString primaryHex = ThemeManager::instance()->primaryColor().name();

    if (m_badgeLabel) {
        m_badgeLabel->setStyleSheet(QString(R"(
            QLabel {
                padding: 0 8px;
                font-size: 11px;
                font-weight: bold;
                color: #ffffff;
                border-radius: 4px;
                background-color: %1;
            }
        )").arg(primaryHex));
    }

    if (dark) {
        if (m_titleLabel) m_titleLabel->setStyleSheet("font-weight: bold; font-size: 13.5px; color: #f5f5f7;");
        if (m_metaLabel) m_metaLabel->setStyleSheet("color: #a1a1a6; font-size: 11px;");
        if (m_textBrowser) {
            m_textBrowser->setStyleSheet(R"(
                QTextBrowser {
                    border: 1px solid rgba(255, 255, 255, 0.08);
                    border-radius: 7px;
                    background: rgba(25, 25, 28, 0.85);
                    padding: 8px;
                    font-family: 'Consolas', monospace;
                    font-size: 12.5px;
                    color: #f5f5f7;
                }
            )");
        }
    } else {
        if (m_titleLabel) m_titleLabel->setStyleSheet("font-weight: bold; font-size: 13.5px; color: #1d1d1f;");
        if (m_metaLabel) m_metaLabel->setStyleSheet("color: #86868b; font-size: 11px;");
        if (m_textBrowser) {
            m_textBrowser->setStyleSheet(R"(
                QTextBrowser {
                    border: 1px solid rgba(0, 0, 0, 0.08);
                    border-radius: 7px;
                    background: rgba(255, 255, 255, 0.88);
                    padding: 8px;
                    font-family: 'Consolas', monospace;
                    font-size: 12.5px;
                    color: #1d1d1f;
                }
            )");
        }
    }
}

void PreviewWindow::showPreview(const ClipboardItem& item, const QRect& anchorRect, const QRect& parentWindowRect, bool isToggle) {
    if (isToggle && isVisible() && m_currentShowingId == item.id) {
        hide();
        m_currentShowingId.clear();
        return;
    }
    m_currentShowingId = item.id;

    int targetW = 400;
    int targetH = 240;

    QString srcApp = item.sourceAppName.isEmpty() ? "系统" : item.sourceAppName;

    if (item.kind == "image") {
        m_titleLabel->setText("图片预览");
        m_badgeLabel->setText("图片");
        m_textBrowser->hide();
        m_imagePreview->show();

        QString imgPath = AppPaths::imageCacheDir() + "/" + item.content;
        QImage img(imgPath);
        if (!img.isNull()) {
            int imgW = img.width();
            int imgH = img.height();
            qint64 fileSize = QFileInfo(imgPath).size();
            QString sizeStr = (fileSize > 1024 * 1024) 
                ? QString::number(fileSize / (1024.0 * 1024.0), 'f', 1) + " MB" 
                : QString::number(fileSize / 1024.0, 'f', 0) + " KB";
            m_metaLabel->setText(QString("%1 x %2 · %3 · %4").arg(imgW).arg(imgH).arg(sizeStr).arg(srcApp));

            int maxPreviewW = 480;
            int maxPreviewH = 400;
            QSize scaledSize = img.size().scaled(maxPreviewW, maxPreviewH, Qt::KeepAspectRatio);
            targetW = qBound(300, scaledSize.width() + 32, 520);
            targetH = qBound(180, scaledSize.height() + 72, 480);
            m_imagePreview->setPixmap(QPixmap::fromImage(img).scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            m_metaLabel->setText(QString("图片无法读取 · %1").arg(srcApp));
            targetW = 320;
            targetH = 180;
        }
    } else if (item.kind == "html") {
        m_titleLabel->setText("HTML 预览");
        m_badgeLabel->setText("HTML");
        m_metaLabel->setText(QString("HTML 富文本 · %1").arg(srcApp));
        m_imagePreview->hide();
        m_textBrowser->show();
        m_textBrowser->setHtml(item.content);
        targetW = 460;
        targetH = 340;
    } else if (item.kind == "files") {
        m_titleLabel->setText("文件预览");
        m_badgeLabel->setText("文件");
        m_metaLabel->setText(QString("本地文件或文件夹 · %1").arg(srcApp));
        m_imagePreview->hide();
        m_textBrowser->show();
        m_textBrowser->setPlainText(item.content);
        targetW = 380;
        targetH = 200;
    } else {
        m_titleLabel->setText("文本预览");
        m_badgeLabel->setText("文本");
        QString txt = item.content;
        int charCount = txt.length();
        int lineCount = txt.count('\n') + 1;
        m_metaLabel->setText(QString("%1 字符 · %2 行 · %3").arg(charCount).arg(lineCount).arg(srcApp));
        m_imagePreview->hide();
        m_textBrowser->show();

        // If lines exceed 20, truncate at 20 lines without any scrollbar
        QStringList allLines = txt.split('\n');
        QString displayTxt = txt;
        if (allLines.size() > 20) {
            displayTxt = allLines.mid(0, 20).join('\n');
        }

        m_textBrowser->setPlainText(displayTxt);
        m_textBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_textBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        QFontMetrics fm(m_textBrowser->font());
        int maxLineWidth = 0;
        const QStringList lines = displayTxt.split('\n');
        for (int i = 0; i < lines.size(); ++i) {
            maxLineWidth = qMax(maxLineWidth, fm.horizontalAdvance(lines[i]));
        }
        targetW = qBound(380, maxLineWidth + 72, 600);
        int contentInnerW = targetW - 36;
        m_textBrowser->document()->setTextWidth(contentInnerW);
        int docH = static_cast<int>(m_textBrowser->document()->size().height());

        int lineH = fm.lineSpacing();
        int max20LinesH = 20 * lineH + 88;
        targetH = qBound(140, docH + 84, max20LinesH);
    }

    setFixedSize(targetW, targetH);

    // Calculate non-overlapping docked position
    QPoint refPoint = parentWindowRect.isValid() ? parentWindowRect.topLeft() : QCursor::pos();
    QScreen* screen = QGuiApplication::screenAt(refPoint);
    if (!screen) screen = QGuiApplication::primaryScreen();
    QRect screenGeo = screen->availableGeometry();

    int posX = 0;
    int posY = 0;

    if (parentWindowRect.isValid()) {
        int leftSpace = parentWindowRect.left() - screenGeo.left();
        if (leftSpace >= targetW + 12) {
            // Position to the LEFT of clipboard window
            posX = parentWindowRect.left() - targetW - 8;
        } else {
            // Position to the RIGHT of clipboard window
            posX = parentWindowRect.right() + 8;
        }

        if (anchorRect.isValid()) {
            posY = anchorRect.center().y() - targetH / 2;
        } else {
            posY = parentWindowRect.center().y() - targetH / 2;
        }
    } else {
        posX = QCursor::pos().x() - targetW - 12;
        posY = QCursor::pos().y() - targetH / 2;
    }

    // Clamp inside screen bounds
    posX = qBound(screenGeo.left() + 6, posX, screenGeo.right() - targetW - 6);
    posY = qBound(screenGeo.top() + 6, posY, screenGeo.bottom() - targetH - 6);

    move(posX, posY);

    if (!isVisible()) {
        setWindowOpacity(0.0);
        show();
        QPropertyAnimation* anim = new QPropertyAnimation(this, "windowOpacity", this);
        anim->setDuration(120);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
    raise();

    HWND hwnd = reinterpret_cast<HWND>(winId());
    bool dark = ThemeManager::instance()->isDarkMode();
    WindowBackdropHelper::enableAcrylic(hwnd, dark);
}

void PreviewWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        hide();
    }
    QWidget::keyPressEvent(event);
}

void PreviewWindow::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space) {
        if (!event->isAutoRepeat()) {
            hide();
        }
        event->accept();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void PreviewWindow::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect r = rect().adjusted(0, 0, -1, -1);
    QPainterPath path;
    path.addRoundedRect(r, 11, 11);

    // Frosted Glass Tint
    painter.fillPath(path, ThemeManager::instance()->windowGlassColor());

    // Dual Border
    painter.setPen(QPen(ThemeManager::instance()->glassBorderColor(), 1));
    painter.drawPath(path);

    QPainterPath innerPath;
    innerPath.addRoundedRect(r.adjusted(1, 1, -1, -1), 10, 10);
    painter.setPen(QPen(ThemeManager::instance()->glassInnerGlowColor(), 1));
    painter.drawPath(innerPath);
}

void PreviewWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    EventBus::instance()->previewVisibilityChanged(true);
    MemoryManager::instance()->notifyWindowShown();
}

void PreviewWindow::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    m_currentShowingId.clear();
    EventBus::instance()->previewVisibilityChanged(false);
    MemoryManager::instance()->notifyWindowHidden();
}
