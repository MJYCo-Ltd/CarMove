#include "DataManagement/PostGisConnection.h"
#include "Core/AppLogger.h"

#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QUuid>

namespace PostGisConnection {

bool ensureDriverAvailable(QString& errorMessage)
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QPSQL"))) {
        errorMessage = QStringLiteral("未找到 PostgreSQL 驱动 (QPSQL)，请确认已安装 Qt SQL 驱动 libpq 插件");
        AppLogger::error(errorMessage);
        return false;
    }
    return true;
}

bool validateIdentifier(const QString& identifier, QString& errorMessage)
{
    static const QRegularExpression identifierPattern(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    if (!identifierPattern.match(identifier).hasMatch()) {
        errorMessage = QStringLiteral("非法 SQL 标识符: %1").arg(identifier);
        AppLogger::warn(errorMessage);
        return false;
    }
    return true;
}

QStringList sqlIdentifiers(const PostGisDatabaseConfig& config)
{
    return {
        config.schema,
        config.trajectoryTable,
        config.trajectoryDaysTable,
        config.vehiclesTable,
        config.plateColumn,
        config.timeColumn,
        config.geomColumn,
        config.speedColumn,
        config.directionColumn,
        config.colorColumn,
    };
}

bool validateConfigIdentifiers(const PostGisDatabaseConfig& config, QString& errorMessage)
{
    errorMessage.clear();
    for (const QString& identifier : sqlIdentifiers(config)) {
        if (!validateIdentifier(identifier, errorMessage)) {
            return false;
        }
    }
    return true;
}

QString makeConnectionName(const QString& prefix)
{
    return QStringLiteral("%1_%2").arg(prefix, QUuid::createUuid().toString(QUuid::WithoutBraces));
}

bool openDatabase(const PostGisDatabaseConfig& config,
                  const QString& connectionName,
                  QString& errorMessage)
{
    errorMessage.clear();

    if (!ensureDriverAvailable(errorMessage)) {
        return false;
    }

    if (!validateConfigIdentifiers(config, errorMessage)) {
        return false;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), connectionName);
    db.setHostName(config.host);
    db.setPort(config.port);
    db.setDatabaseName(config.database);
    db.setUserName(config.username);
    db.setPassword(config.password);

    if (!db.open()) {
        errorMessage = QStringLiteral("连接数据库失败: %1").arg(db.lastError().text());
        AppLogger::error(QStringLiteral("PostGIS 连接失败: %1@%2:%3/%4 | %5")
                             .arg(config.username,
                                  config.host,
                                  QString::number(config.port),
                                  config.database,
                                  db.lastError().text()));
        QSqlDatabase::removeDatabase(connectionName);
        return false;
    }

    return true;
}

void closeDatabase(const QString& connectionName)
{
    if (connectionName.isEmpty() || !QSqlDatabase::contains(connectionName)) {
        return;
    }

    {
        QSqlDatabase db = QSqlDatabase::database(connectionName);
        if (db.isOpen()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
}

} // namespace PostGisConnection
