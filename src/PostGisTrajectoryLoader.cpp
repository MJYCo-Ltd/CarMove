#include "PostGisTrajectoryLoader.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>
#include <QVariant>

namespace {

QString colorFromPlateColorField(const QString& raw)
{
    if (raw.contains(QStringLiteral("黄"), Qt::CaseInsensitive)) {
        return QStringLiteral("yellow");
    }
    return QStringLiteral("blue");
}

} // namespace

PostGisTrajectoryLoader::PostGisTrajectoryLoader(QObject* parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("carmove_postgis_%1").arg(QUuid::createUuid().toString()))
{
}

PostGisTrajectoryLoader::~PostGisTrajectoryLoader()
{
    disconnectDatabase();
}

bool PostGisTrajectoryLoader::validateIdentifier(const QString& identifier, QString& errorMessage) const
{
    static const QRegularExpression identifierPattern(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    if (!identifierPattern.match(identifier).hasMatch()) {
        errorMessage = QStringLiteral("非法 SQL 标识符: %1").arg(identifier);
        return false;
    }
    return true;
}

bool PostGisTrajectoryLoader::ensureDriverAvailable(QString& errorMessage) const
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QPSQL"))) {
        errorMessage = QStringLiteral("未找到 PostgreSQL 驱动 (QPSQL)，请确认已安装 Qt SQL 驱动 libpq 插件");
        return false;
    }
    return true;
}

QString PostGisTrajectoryLoader::quotedIdentifier(const QString& identifier) const
{
    return QLatin1Char('"') + identifier + QLatin1Char('"');
}

QString PostGisTrajectoryLoader::qualifiedTable(const QString& tableName) const
{
    return quotedIdentifier(m_config.schema) + QLatin1Char('.') + quotedIdentifier(tableName);
}

bool PostGisTrajectoryLoader::connectDatabase(const PostGisDatabaseConfig& config, QString& errorMessage)
{
    errorMessage.clear();
    disconnectDatabase();

    m_config = config;
    if (!m_config.isValid()) {
        errorMessage = QStringLiteral("PostGIS 数据库配置不完整");
        return false;
    }

    if (!ensureDriverAvailable(errorMessage)) {
        return false;
    }

    const QStringList identifiers = {
        m_config.schema,
        m_config.trajectoryTable,
        m_config.vehiclesTable,
        m_config.plateColumn,
        m_config.timeColumn,
        m_config.geomColumn,
        m_config.speedColumn,
        m_config.directionColumn,
        m_config.mileageColumn,
        m_config.colorColumn,
    };
    for (const QString& identifier : identifiers) {
        if (!validateIdentifier(identifier, errorMessage)) {
            return false;
        }
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), m_connectionName);
    db.setHostName(m_config.host);
    db.setPort(m_config.port);
    db.setDatabaseName(m_config.database);
    db.setUserName(m_config.username);
    db.setPassword(m_config.password);

    if (!db.open()) {
        errorMessage = QStringLiteral("连接数据库失败: %1").arg(db.lastError().text());
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }

    m_connected = true;
    return true;
}

void PostGisTrajectoryLoader::disconnectDatabase()
{
    if (m_connectionName.isEmpty()) {
        return;
    }

    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName);
            if (db.isOpen()) {
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }

    m_connected = false;
}

QList<FolderScanner::VehicleInfo> PostGisTrajectoryLoader::listVehicles(QString& errorMessage) const
{
    QList<FolderScanner::VehicleInfo> vehicles;
    errorMessage.clear();

    if (!m_connected || !QSqlDatabase::contains(m_connectionName)) {
        errorMessage = QStringLiteral("数据库未连接");
        return vehicles;
    }

    const QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    const QString trajectoryTable = qualifiedTable(m_config.trajectoryTable);
    const QString plateColumn = quotedIdentifier(m_config.plateColumn);
    const QString timeColumn = quotedIdentifier(m_config.timeColumn);

    const QString sql = QStringLiteral(
                            "SELECT %1 AS plate_number, MIN(%2) AS first_ts, MAX(%2) AS last_ts, COUNT(*) AS point_count "
                            "FROM %3 "
                            "GROUP BY %1 "
                            "ORDER BY %1")
                            .arg(plateColumn, timeColumn, trajectoryTable);

    QSqlQuery query(db);
    if (!query.exec(sql)) {
        errorMessage = QStringLiteral("查询车辆列表失败: %1").arg(query.lastError().text());
        return vehicles;
    }

    while (query.next()) {
        FolderScanner::VehicleInfo info;
        info.plateNumber = query.value(QStringLiteral("plate_number")).toString().trimmed();
        info.firstTimestamp = query.value(QStringLiteral("first_ts")).toDateTime();
        info.lastTimestamp = query.value(QStringLiteral("last_ts")).toDateTime();
        info.recordCount = query.value(QStringLiteral("point_count")).toInt();
        if (!info.plateNumber.isEmpty()) {
            vehicles.append(info);
        }
    }

    if (vehicles.isEmpty()) {
        errorMessage = QStringLiteral("数据库中没有轨迹数据");
    }

    return vehicles;
}

QList<ExcelDataReader::VehicleRecord> PostGisTrajectoryLoader::loadTrajectory(const QString& plateNumber,
                                                                              QString& errorMessage) const
{
    QList<ExcelDataReader::VehicleRecord> records;
    errorMessage.clear();

    if (plateNumber.trimmed().isEmpty()) {
        errorMessage = QStringLiteral("车牌号为空");
        return records;
    }

    if (!m_connected || !QSqlDatabase::contains(m_connectionName)) {
        errorMessage = QStringLiteral("数据库未连接");
        return records;
    }

    const QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    const QString trajectoryTable = qualifiedTable(m_config.trajectoryTable);
    const QString vehiclesTable = qualifiedTable(m_config.vehiclesTable);
    const QString plateCol = quotedIdentifier(m_config.plateColumn);
    const QString timeCol = quotedIdentifier(m_config.timeColumn);
    const QString geomCol = quotedIdentifier(m_config.geomColumn);
    const QString speedCol = quotedIdentifier(m_config.speedColumn);
    const QString directionCol = quotedIdentifier(m_config.directionColumn);
    const QString mileageCol = quotedIdentifier(m_config.mileageColumn);
    const QString colorCol = quotedIdentifier(m_config.colorColumn);

    const QString sql = QStringLiteral(
                            "SELECT tp.%1 AS plate_number, "
                            "ST_X(tp.%2) AS longitude, ST_Y(tp.%2) AS latitude, "
                            "tp.%3 AS ts, tp.%4 AS speed, tp.%5 AS direction, tp.%6 AS total_mileage, "
                            "COALESCE(v.%7, '') AS plate_color "
                            "FROM %8 tp "
                            "LEFT JOIN %9 v ON v.%1 = tp.%1 "
                            "WHERE tp.%1 = :plate "
                            "ORDER BY tp.%3")
                            .arg(plateCol,
                                 geomCol,
                                 timeCol,
                                 speedCol,
                                 directionCol,
                                 mileageCol,
                                 colorCol,
                                 trajectoryTable,
                                 vehiclesTable);

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(QStringLiteral(":plate"), plateNumber.trimmed());

    if (!query.exec()) {
        errorMessage = QStringLiteral("查询轨迹失败: %1").arg(query.lastError().text());
        return records;
    }

    while (query.next()) {
        ExcelDataReader::VehicleRecord record;
        record.plateNumber = query.value(QStringLiteral("plate_number")).toString().trimmed();
        record.longitude = query.value(QStringLiteral("longitude")).toDouble();
        record.latitude = query.value(QStringLiteral("latitude")).toDouble();
        record.timestamp = query.value(QStringLiteral("ts")).toDateTime();
        record.speed = query.value(QStringLiteral("speed")).toDouble();
        record.direction = query.value(QStringLiteral("direction")).toInt();
        record.totalMileage = query.value(QStringLiteral("total_mileage")).toString();
        record.vehicleColor = colorFromPlateColorField(query.value(QStringLiteral("plate_color")).toString());

        if (record.isValid()) {
            records.append(record);
        }
    }

    if (records.isEmpty()) {
        errorMessage = QStringLiteral("未找到车辆 %1 的轨迹数据").arg(plateNumber);
    }

    return records;
}
