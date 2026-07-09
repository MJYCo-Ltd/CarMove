#ifndef APPLOGGER_H
#define APPLOGGER_H

#include <QString>

class AppLogger
{
public:
    static void initialize();
    static void shutdown();

    static bool isInitialized();
    static QString logDirectory();

    static void trace(const QString& message);
    static void debug(const QString& message);
    static void info(const QString& message);
    static void warn(const QString& message);
    static void error(const QString& message);
    static void critical(const QString& message);

private:
    static bool s_initialized;
};

#endif // APPLOGGER_H
