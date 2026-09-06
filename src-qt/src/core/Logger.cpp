#include "Logger.h"
#include "AppPaths.h"
#include <QDateTime>
#include <QTextStream>
#include <iostream>

QFile Logger::s_logFile;
QMutex Logger::s_mutex;

void Logger::init() {
    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    QString filePath = AppPaths::logDir() + QString("/ecopaste_%1.log").arg(today);
    
    s_logFile.setFileName(filePath);
    s_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void Logger::log(const QString& level, const QString& message) {
    QMutexLocker locker(&s_mutex);
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString formatted = QString("[%1] [%2] %3\n").arg(timestamp, level, message);

    // Print to console
    std::cout << formatted.toStdString();
    std::cout.flush();

    // Write to file
    if (s_logFile.isOpen()) {
        s_logFile.write(formatted.toUtf8());
        s_logFile.flush();
    }
}
