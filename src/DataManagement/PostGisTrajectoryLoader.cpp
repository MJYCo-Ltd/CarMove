#include "DataManagement/PostGisTrajectoryLoader.h"
#include "DataManagement/PostGisConnection.h"
#include "DataManagement/PostGisSqlHelpers.h"
#include "Core/AppLogger.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
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

bool PostGisTrajectoryLoader::connectDatabase(const PostGisDatabaseConfig& config, QString& errorMessage)
{
    errorMessage.clear();
    disconnectDatabase();

    m_config = config;
    if (!m_config.isValid()) {
        errorMessage = QStringLiteral("PostGIS 数据库配置不完整");
        AppLogger::error(errorMessage);
        return false;
    }

    if (!PostGisConnection::openDatabase(m_config, m_connectionName, errorMessage)) {
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

    PostGisConnection::closeDatabase(m_connectionName);
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
    const QString vehiclesTable = PostGisSql::qualifiedTable(m_config, m_config.vehiclesTable);
    const QString plateColumn = PostGisSql::quotedIdentifier(m_config.plateColumn);

    const QString sql = QStringLiteral(
                            "SELECT %1 AS plate_number, first_seen_at, last_seen_at, "
                            "total_point_count, day_count "
                            "FROM %2 "
                            "WHERE day_count > 0 OR total_point_count > 0 "
                            "ORDER BY %1")
                            .arg(plateColumn, vehiclesTable);

    QSqlQuery query(db);
    if (!query.exec(sql)) {
        errorMessage = QStringLiteral("查询车辆列表失败: %1").arg(query.lastError().text());
        return vehicles;
    }

    while (query.next()) {
        FolderScanner::VehicleInfo info;
        info.plateNumber = query.value(QStringLiteral("plate_number")).toString().trimmed();
        info.firstTimestamp = query.value(QStringLiteral("first_seen_at")).toDateTime();
        info.lastTimestamp = query.value(QStringLiteral("last_seen_at")).toDateTime();
        info.recordCount = query.value(QStringLiteral("total_point_count")).toInt();
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
                                                                              QString& errorMessage,
                                                                              const QDate& startDate,
                                                                              const QDate& endDate) const
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
    const QString trajectoryTable = PostGisSql::qualifiedTable(m_config, m_config.trajectoryTable);
    const QString trajectoryDaysTable = PostGisSql::qualifiedTable(m_config, m_config.trajectoryDaysTable);
    const QString vehiclesTable = PostGisSql::qualifiedTable(m_config, m_config.vehiclesTable);
    const QString plateCol = PostGisSql::quotedIdentifier(m_config.plateColumn);
    const QString timeCol = PostGisSql::quotedIdentifier(m_config.timeColumn);
    const QString geomCol = PostGisSql::quotedIdentifier(m_config.geomColumn);
    const QString speedCol = PostGisSql::quotedIdentifier(m_config.speedColumn);
    const QString directionCol = PostGisSql::quotedIdentifier(m_config.directionColumn);
    const QString colorCol = PostGisSql::quotedIdentifier(m_config.colorColumn);

    QString dateFilterSql;
    if (startDate.isValid() && endDate.isValid()) {
        dateFilterSql = QStringLiteral("AND td.trajectory_date >= :start_date AND td.trajectory_date <= :end_date ");
    }

    const QString sql = QStringLiteral(
                            "SELECT v.%1 AS plate_number, "
                            "td.trajectory_date, "
                            "ST_X(tp.%2) AS longitude, ST_Y(tp.%2) AS latitude, "
                            "tp.%3 AS point_ts, tp.%4 AS speed, tp.%5 AS direction, "
                            "COALESCE(v.%6, '') AS plate_color "
                            "FROM %7 tp "
                            "JOIN %8 td ON td.trajectory_day_id = tp.trajectory_day_id "
                            "JOIN %9 v ON v.vehicle_id = td.vehicle_id "
                            "WHERE v.%1 = :plate "
                            "%10"
                            "ORDER BY td.trajectory_date, tp.%3")
                            .arg(plateCol,
                                 geomCol,
                                 timeCol,
                                 speedCol,
                                 directionCol,
                                 colorCol,
                                 trajectoryTable,
                                 trajectoryDaysTable,
                                 vehiclesTable,
                                 dateFilterSql);

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(QStringLiteral(":plate"), plateNumber.trimmed());
    if (startDate.isValid() && endDate.isValid()) {
        query.bindValue(QStringLiteral(":start_date"), startDate);
        query.bindValue(QStringLiteral(":end_date"), endDate);
    }

    if (!query.exec()) {
        errorMessage = QStringLiteral("查询轨迹失败: %1").arg(query.lastError().text());
        AppLogger::error(QStringLiteral("查询轨迹失败: 车牌=%1 | %2").arg(plateNumber, errorMessage));
        return records;
    }

    while (query.next()) {
        ExcelDataReader::VehicleRecord record;
        record.plateNumber = query.value(QStringLiteral("plate_number")).toString().trimmed();
        record.longitude = query.value(QStringLiteral("longitude")).toDouble();
        record.latitude = query.value(QStringLiteral("latitude")).toDouble();
        record.timestamp = PostGisSql::calendarDateTime(
            query.value(QStringLiteral("trajectory_date")).toDate(),
            query.value(QStringLiteral("point_ts")).toTime());
        record.speed = query.value(QStringLiteral("speed")).toDouble();
        record.direction = query.value(QStringLiteral("direction")).toInt();
        record.vehicleColor = colorFromPlateColorField(query.value(QStringLiteral("plate_color")).toString());

        if (record.isValid()) {
            records.append(record);
        }
    }

    if (records.isEmpty()) {
        errorMessage = QStringLiteral("未找到车辆 %1 的轨迹数据").arg(plateNumber);
        const QString periodHint = (startDate.isValid() && endDate.isValid())
                                       ? QStringLiteral(" | 时段=%1~%2")
                                             .arg(startDate.toString(Qt::ISODate),
                                                  endDate.toString(Qt::ISODate))
                                       : QString();
        AppLogger::warn(QStringLiteral("加载轨迹为空: 车牌=%1%2").arg(plateNumber, periodHint));
    }

    return records;
}
