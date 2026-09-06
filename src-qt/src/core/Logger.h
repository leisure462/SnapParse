#pragma once

#include <QString>
#include <QDebug>
#include <QFile>
#include <QMutex>

class Logger {
public:
    static void init();
    static void log(const QString& level, const QString& message);

    static void info(const QString& msg) { log("INFO", msg); }
    static void warn(const QString& msg) { log("WARN", msg); }
    static void error(const QString& msg) { log("ERROR", msg); }
    static void debug(const QString& msg) { log("DEBUG", msg); }

private:
    static QFile s_logFile;
    static QMutex s_mutex;
};
