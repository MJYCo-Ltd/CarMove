#include "Core/AppLogger.h"

#include <QCoreApplication>
#include <QDir>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {

std::string toUtf8(const QString& text)
{
    return text.toUtf8().constData();
}

QtMessageHandler s_previousQtMessageHandler = nullptr;
thread_local bool s_inQtMessageHandler = false;

void writeBootstrapMessage(const char* level, const char* message)
{
    std::fprintf(stderr, "[%s] %s\n", level, message);
}

void writeSpdlogDirect(spdlog::level::level_enum level, const QString& message)
{
    if (!AppLogger::isInitialized()) {
        const char* levelName = "info";
        switch (level) {
        case spdlog::level::trace:
        case spdlog::level::debug:
            levelName = "debug";
            break;
        case spdlog::level::warn:
            levelName = "warn";
            break;
        case spdlog::level::err:
            levelName = "error";
            break;
        case spdlog::level::critical:
            levelName = "critical";
            break;
        default:
            break;
        }
        writeBootstrapMessage(levelName, toUtf8(message).c_str());
        return;
    }

    spdlog::log(level, "{}", toUtf8(message));
}

void qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (s_inQtMessageHandler) {
        return;
    }

    s_inQtMessageHandler = true;

    QString fullMessage = message;
    if (context.file != nullptr && context.line > 0) {
        fullMessage += QStringLiteral(" (%1:%2)")
                           .arg(QString::fromUtf8(context.file))
                           .arg(context.line);
    } else if (context.category != nullptr && context.category[0] != '\0') {
        fullMessage += QStringLiteral(" [%1]").arg(QString::fromUtf8(context.category));
    }

    switch (type) {
    case QtDebugMsg:
        writeSpdlogDirect(spdlog::level::debug, fullMessage);
        break;
    case QtInfoMsg:
        writeSpdlogDirect(spdlog::level::info, fullMessage);
        break;
    case QtWarningMsg:
        writeSpdlogDirect(spdlog::level::warn, fullMessage);
        break;
    case QtCriticalMsg:
        writeSpdlogDirect(spdlog::level::critical, fullMessage);
        break;
    case QtFatalMsg:
        writeSpdlogDirect(spdlog::level::critical, fullMessage);
        s_inQtMessageHandler = false;
        std::abort();
    }

    s_inQtMessageHandler = false;
}

} // namespace

bool AppLogger::s_initialized = false;

bool AppLogger::isInitialized()
{
    return s_initialized;
}

QString AppLogger::logDirectory()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("log"));
}

void AppLogger::initialize()
{
    if (s_initialized) {
        return;
    }

    const QString logDirPath = logDirectory();
    QDir().mkpath(logDirPath);

    const std::string logFile =
        QDir(logDirPath).filePath(QStringLiteral("carmove.log")).toUtf8().constData();

    try {
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto fileSink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(logFile, 0, 0, false, 7);

        std::vector<spdlog::sink_ptr> sinks {consoleSink, fileSink};
        auto logger = std::make_shared<spdlog::logger>(QStringLiteral("CarMove").toUtf8().constData(),
                                                       sinks.begin(),
                                                       sinks.end());
        logger->set_level(spdlog::level::trace);
        logger->flush_on(spdlog::level::warn);
        spdlog::set_default_logger(logger);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

        s_initialized = true;
        s_previousQtMessageHandler = qInstallMessageHandler(qtMessageHandler);

        const QString appName = QCoreApplication::applicationName();
        const QString appVersion = QCoreApplication::applicationVersion();
        spdlog::info("日志系统已启动 | 应用={} | 版本={} | 目录={}",
                     toUtf8(appName),
                     toUtf8(appVersion),
                     toUtf8(logDirPath));
    } catch (const spdlog::spdlog_ex& ex) {
        writeBootstrapMessage("error", ex.what());
    }
}

void AppLogger::shutdown()
{
    if (!s_initialized) {
        return;
    }

    spdlog::info("日志系统关闭");
    qInstallMessageHandler(s_previousQtMessageHandler);
    s_previousQtMessageHandler = nullptr;
    spdlog::shutdown();
    s_initialized = false;
}

void AppLogger::trace(const QString& message)
{
    writeSpdlogDirect(spdlog::level::trace, message);
}

void AppLogger::debug(const QString& message)
{
    writeSpdlogDirect(spdlog::level::debug, message);
}

void AppLogger::info(const QString& message)
{
    writeSpdlogDirect(spdlog::level::info, message);
}

void AppLogger::warn(const QString& message)
{
    writeSpdlogDirect(spdlog::level::warn, message);
}

void AppLogger::error(const QString& message)
{
    writeSpdlogDirect(spdlog::level::err, message);
}

void AppLogger::critical(const QString& message)
{
    writeSpdlogDirect(spdlog::level::critical, message);
}
