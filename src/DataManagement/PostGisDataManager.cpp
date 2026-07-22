#include "DataManagement/PostGisDataManager.h"
#include "DataManagement/PostGisConnection.h"
#include "DataManagement/PostGisSqlHelpers.h"
#include "Core/AppLogger.h"

#include "ExcelDriver/ExcelParserManager.h"
#include "Core/LocalFilePath.h"
#include "DataParsing/TrajectoryFileNaming.h"
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QMap>
#include <QSet>
#include <algorithm>

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QTime>
#include <QUuid>
#include <QVariant>

namespace {

QString normalizePlateColor(const TrajectoryPoint& record)
{
    if (record.vehicleColor == QStringLiteral("yellow") || record.vehicleColor == QStringLiteral("blue")) {
        return record.vehicleColor;
    }
    if (record.vehicleColor.contains(QStringLiteral("黄"), Qt::CaseInsensitive)) {
        return QStringLiteral("yellow");
    }
    return record.vehicleColor.isEmpty() ? QString() : record.vehicleColor;
}

void logImportSkipped(const TrajectoryFileInfo& file,
                      const QString& plateNumber,
                      const QDate& periodStart,
                      const QDate& periodEnd,
                      qint64 existingDays)
{
    AppLogger::info(QStringLiteral("导入跳过: 文件=%1 | 车牌=%2 | 时段=%3~%4 | 已有 %5 天轨迹")
                        .arg(file.fileName,
                             plateNumber,
                             periodStart.toString(Qt::ISODate),
                             periodEnd.toString(Qt::ISODate))
                        .arg(existingDays));
}

QString colorFromPlateColorField(const QString& raw)
{
    if (raw.contains(QStringLiteral("黄"), Qt::CaseInsensitive)) {
        return QStringLiteral("yellow");
    }
    return QStringLiteral("blue");
}

} // namespace

PostGisDataManager::PostGisDataManager(QObject* parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("carmove_postgis_%1").arg(QUuid::createUuid().toString()))
{
}

PostGisDataManager::~PostGisDataManager()
{
    disconnectDatabase();
}

bool PostGisDataManager::isConnected() const
{
    return m_connected;
}

PostGisDatabaseConfig PostGisDataManager::config() const
{
    return m_config;
}

QList<VehicleDayTrajectory> PostGisDataManager::loadTrajectoryByDay(const QString& plateNumber,
                                                                    QString& errorMessage,
                                                                    const QDate& startDate,
                                                                    const QDate& endDate) const
{
    const QDateTime startDateTime = startDate.isValid()
                                        ? QDateTime(startDate, QTime(0, 0, 0))
                                        : QDateTime();
    const QDateTime endDateTime = endDate.isValid()
                                      ? QDateTime(endDate, QTime(23, 59, 59))
                                      : QDateTime();
    const QList<TrajectoryPoint> points =
        loadTrajectoryPoints(plateNumber, errorMessage, startDateTime, endDateTime);
    if (points.isEmpty() && !errorMessage.isEmpty()) {
        return {};
    }
    return groupPointsByDay(points);
}

bool PostGisDataManager::connectDatabase(const PostGisDatabaseConfig& config, QString& errorMessage)
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

void PostGisDataManager::disconnectDatabase()
{
    if (m_connectionName.isEmpty()) {
        return;
    }

    PostGisConnection::closeDatabase(m_connectionName);
    m_connected = false;
}

QList<VehicleSummary> PostGisDataManager::listVehicles(QString& errorMessage) const
{
    QList<VehicleSummary> vehicles;
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
        VehicleSummary info;
        info.plateNumber = query.value(QStringLiteral("plate_number")).toString().trimmed();
        // first/last_seen_at 为 timestamptz，转成上海墙钟，与轨迹点 DATE+TIME 一致
        const QDateTime firstSeen = query.value(QStringLiteral("first_seen_at")).toDateTime();
        const QDateTime lastSeen = query.value(QStringLiteral("last_seen_at")).toDateTime();
        if (firstSeen.isValid()) {
            const QDateTime shanghai = firstSeen.toTimeZone(PostGisSql::shanghaiTimeZone());
            info.firstSeenAt = PostGisSql::calendarDateTime(shanghai.date(), shanghai.time());
        }
        if (lastSeen.isValid()) {
            const QDateTime shanghai = lastSeen.toTimeZone(PostGisSql::shanghaiTimeZone());
            info.lastSeenAt = PostGisSql::calendarDateTime(shanghai.date(), shanghai.time());
        }
        info.totalPointCount = query.value(QStringLiteral("total_point_count")).toLongLong();
        info.dayCount = query.value(QStringLiteral("day_count")).toInt();
        if (!info.plateNumber.isEmpty()) {
            vehicles.append(info);
        }
    }

    if (vehicles.isEmpty()) {
        errorMessage = QStringLiteral("数据库中没有轨迹数据");
    }

    return vehicles;
}

QList<TrajectoryPoint> PostGisDataManager::loadTrajectoryPoints(const QString& plateNumber,
                                                               QString& errorMessage,
                                                               const QDateTime& startDateTime,
                                                               const QDateTime& endDateTime) const
{
    QList<TrajectoryPoint> records;
    errorMessage.clear();

    if (plateNumber.trimmed().isEmpty()) {
        errorMessage = QStringLiteral("车牌号为空");
        return records;
    }

    if (!m_connected || !QSqlDatabase::contains(m_connectionName)) {
        errorMessage = QStringLiteral("数据库未连接");
        return records;
    }

    const bool hasTimeRange = startDateTime.isValid() && endDateTime.isValid();
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
    if (hasTimeRange) {
        // 点表时间为「日历日 + TIME」墙钟，勿直接绑定带时区的 QDateTime（Qt/PSQL 常按 UTC
        // 会话换算，会把当天下午点截掉）。用无时区 timestamp 文本比较。
        dateFilterSql = QStringLiteral(
                            "AND td.trajectory_date >= :start_date AND td.trajectory_date <= :end_date "
                            "AND (td.trajectory_date + tp.%1) >= CAST(:start_ts AS timestamp) "
                            "AND (td.trajectory_date + tp.%1) <= CAST(:end_ts AS timestamp) ")
                            .arg(timeCol);
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
    if (hasTimeRange) {
        const QDateTime startWall = startDateTime.toTimeZone(PostGisSql::shanghaiTimeZone());
        const QDateTime endWall = endDateTime.toTimeZone(PostGisSql::shanghaiTimeZone());
        const QDate startDate = startWall.isValid() ? startWall.date() : startDateTime.date();
        const QDate endDate = endWall.isValid() ? endWall.date() : endDateTime.date();
        const QString startTsText =
            (startWall.isValid() ? startWall : startDateTime).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        const QString endTsText =
            (endWall.isValid() ? endWall : endDateTime).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

        query.bindValue(QStringLiteral(":start_date"), startDate);
        query.bindValue(QStringLiteral(":end_date"), endDate);
        query.bindValue(QStringLiteral(":start_ts"), startTsText);
        query.bindValue(QStringLiteral(":end_ts"), endTsText);
    }

    if (!query.exec()) {
        errorMessage = QStringLiteral("查询轨迹失败: %1").arg(query.lastError().text());
        AppLogger::error(QStringLiteral("查询轨迹失败: 车牌=%1 | %2").arg(plateNumber, errorMessage));
        return records;
    }

    while (query.next()) {
        TrajectoryPoint record;
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
        const QString periodHint = hasTimeRange
                                       ? QStringLiteral(" | 时段=%1~%2")
                                             .arg(startDateTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                                                  endDateTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
                                       : QString();
        AppLogger::warn(QStringLiteral("加载轨迹为空: 车牌=%1%2").arg(plateNumber, periodHint));
    }

    return records;
}

QList<TrajectoryFileInfo> PostGisDataManager::collectTrajectoryFiles(const QString& folderPath) const
{
    QList<TrajectoryFileInfo> files;
    const QString localPath = LocalFilePath::normalizeLocalFilePath(folderPath);
    if (localPath.isEmpty()) {
        return files;
    }

    QDirIterator iterator(localPath,
                          TrajectoryFileNaming::excelFileFilters(),
                          QDir::Files,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString filePath = iterator.next();
        const QFileInfo fileInfo(filePath);
        if (fileInfo.size() == 0) {
            AppLogger::warn(QStringLiteral("扫描跳过: 空文件 %1").arg(filePath));
            continue;
        }
        if (fileInfo.size() > 500LL * 1024 * 1024) {
            AppLogger::warn(QStringLiteral("扫描跳过: 文件过大 (%1 MB) %2")
                                .arg(fileInfo.size() / (1024 * 1024))
                                .arg(filePath));
            continue;
        }

        const TrajectoryFileInfo parsed =
            TrajectoryFileNaming::parseFileName(filePath, TrajectoryFileNaming::ParseMode::AllPatterns);
        if (!parsed.plateNumber.isEmpty()) {
            files.append(parsed);
        } else {
            AppLogger::warn(QStringLiteral("扫描跳过: 文件名无法解析车牌 %1").arg(fileInfo.fileName()));
        }
    }

    return files;
}

qint64 PostGisDataManager::countExistingDays(QSqlDatabase& db,
                                                    const PostGisDatabaseConfig& config,
                                                    const QString& plateNumber,
                                                    const QDate& periodStart,
                                                    const QDate& periodEnd,
                                                    QString& errorMessage) const
{
    if (!periodStart.isValid() || !periodEnd.isValid()) {
        return -1;
    }

    const QString trajectoryDaysTable = PostGisSql::qualifiedTable(config, config.trajectoryDaysTable);
    const QString vehiclesTable = PostGisSql::qualifiedTable(config, config.vehiclesTable);
    const QString plateColumn = PostGisSql::quotedIdentifier(config.plateColumn);

    const QString sql = QStringLiteral(
                            "SELECT COUNT(*) FROM %1 td "
                            "JOIN %2 v ON v.vehicle_id = td.vehicle_id "
                            "WHERE v.%3 = :plate "
                            "AND td.trajectory_date >= :start_date "
                            "AND td.trajectory_date <= :end_date")
                            .arg(trajectoryDaysTable, vehiclesTable, plateColumn);

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(QStringLiteral(":plate"), plateNumber);
    query.bindValue(QStringLiteral(":start_date"), periodStart);
    query.bindValue(QStringLiteral(":end_date"), periodEnd);

    if (!query.exec() || !query.next()) {
        errorMessage = QStringLiteral("检查已有日航迹失败: %1").arg(query.lastError().text());
        return -1;
    }

    return query.value(0).toLongLong();
}

void PostGisDataManager::applyImportSessionSettings(QSqlDatabase& db) const
{
    QSqlQuery query(db);
    query.exec(QStringLiteral("SET synchronous_commit TO OFF"));
    query.exec(QStringLiteral("SET jit TO off"));
}

bool PostGisDataManager::insertTrajectoryDays(QSqlDatabase& db,
                                                     const PostGisDatabaseConfig& config,
                                                     qint64 vehicleId,
                                                     const QMap<QDate, QList<TrajectoryPoint>>& dayBuckets,
                                                     QMap<QDate, qint64>& trajectoryDayIds,
                                                     QString& errorMessage) const
{
    trajectoryDayIds.clear();

    QStringList valuePlaceholders;
    QVariantList bindValues;
    valuePlaceholders.reserve(dayBuckets.size());

    for (auto it = dayBuckets.constBegin(); it != dayBuckets.constEnd(); ++it) {
        const QList<TrajectoryPoint>& dayRecords = it.value();
        if (dayRecords.isEmpty()) {
            continue;
        }

        valuePlaceholders.append(QStringLiteral("(?, ?, ?, ?, ?)"));
        bindValues.append(vehicleId);
        bindValues.append(it.key());
        bindValues.append(PostGisSql::calendarTime(dayRecords.first().timestamp));
        bindValues.append(PostGisSql::calendarTime(dayRecords.last().timestamp));
        bindValues.append(dayRecords.size());
    }

    if (valuePlaceholders.isEmpty()) {
        return true;
    }

    const QString trajectoryDaysTable = PostGisSql::qualifiedTable(config, config.trajectoryDaysTable);
    const QString sql = QStringLiteral(
                            "INSERT INTO %1 (vehicle_id, trajectory_date, first_ts, last_ts, point_count) "
                            "VALUES %2 "
                            "RETURNING trajectory_day_id, trajectory_date")
                            .arg(trajectoryDaysTable, valuePlaceholders.join(QStringLiteral(", ")));

    QSqlQuery query(db);
    query.prepare(sql);
    for (const QVariant& value : bindValues) {
        query.addBindValue(value);
    }

    if (!query.exec()) {
        errorMessage = QStringLiteral("批量写入日航迹失败: %1").arg(query.lastError().text());
        return false;
    }

    while (query.next()) {
        trajectoryDayIds.insert(query.value(QStringLiteral("trajectory_date")).toDate(),
                                query.value(QStringLiteral("trajectory_day_id")).toLongLong());
    }

    if (trajectoryDayIds.size() != valuePlaceholders.size()) {
        errorMessage = QStringLiteral("批量写入日航迹失败: 返回的日航迹数量不完整");
        return false;
    }

    return true;
}

bool PostGisDataManager::insertTrajectoryBatch(QSqlDatabase& db,
                                                      const PostGisDatabaseConfig& config,
                                                      const QList<TrajectoryPoint>& records,
                                                      qint64 trajectoryDayId,
                                                      QString& errorMessage) const
{
    static constexpr qsizetype batchSize = 5000;

    const QString trajectoryTable = PostGisSql::qualifiedTable(config, config.trajectoryTable);
    const QString insertPrefix = QStringLiteral(
                                     "INSERT INTO %1 (trajectory_day_id, %2, %3, %4, %5) VALUES ")
                                     .arg(trajectoryTable,
                                          PostGisSql::quotedIdentifier(config.timeColumn),
                                          PostGisSql::quotedIdentifier(config.geomColumn),
                                          PostGisSql::quotedIdentifier(config.speedColumn),
                                          PostGisSql::quotedIdentifier(config.directionColumn));

    for (qsizetype offset = 0; offset < records.size(); offset += batchSize) {
        const qsizetype currentBatchSize = std::min(batchSize, records.size() - offset);
        QStringList placeholders;
        placeholders.reserve(currentBatchSize);
        for (qsizetype i = 0; i < currentBatchSize; ++i) {
            placeholders.append(QStringLiteral("(?, ?, ST_SetSRID(ST_MakePoint(?, ?), 4326), ?, ?)"));
        }

        QSqlQuery query(db);
        query.prepare(insertPrefix + placeholders.join(QStringLiteral(", ")));

        for (qsizetype i = 0; i < currentBatchSize; ++i) {
            const TrajectoryPoint& record = records.at(offset + i);
            query.addBindValue(trajectoryDayId);
            query.addBindValue(PostGisSql::calendarTime(record.timestamp));
            query.addBindValue(record.longitude);
            query.addBindValue(record.latitude);
            query.addBindValue(record.speed);
            query.addBindValue(record.direction);
        }

        if (!query.exec()) {
            errorMessage = QStringLiteral("批量写入轨迹点失败: %1").arg(query.lastError().text());
            return false;
        }
    }

    return true;
}

qint64 PostGisDataManager::upsertVehicle(QSqlDatabase& db,
                                                const PostGisDatabaseConfig& config,
                                                const QString& plateNumber,
                                                const QString& plateColor,
                                                QString& errorMessage) const
{
    const QString vehiclesTable = PostGisSql::qualifiedTable(config, config.vehiclesTable);
    const QString sql = QStringLiteral(
                            "INSERT INTO %1 (%2, %3) VALUES (:plate, :color) "
                            "ON CONFLICT (%2) DO UPDATE SET "
                            "%3 = COALESCE(EXCLUDED.%3, %1.%3), updated_at = now() "
                            "RETURNING vehicle_id")
                            .arg(vehiclesTable,
                                 PostGisSql::quotedIdentifier(QStringLiteral("plate_number")),
                                 PostGisSql::quotedIdentifier(config.colorColumn));

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(QStringLiteral(":plate"), plateNumber);
    query.bindValue(QStringLiteral(":color"), plateColor);

    if (!query.exec()) {
        errorMessage = QStringLiteral("写入车辆主数据失败: %1").arg(query.lastError().text());
        return -1;
    }

    if (!query.next()) {
        errorMessage = QStringLiteral("写入车辆主数据失败: 未返回 vehicle_id");
        return -1;
    }

    return query.value(0).toLongLong();
}

bool PostGisDataManager::refreshVehicleStatsBatch(QSqlDatabase& db,
                                                         const PostGisDatabaseConfig& config,
                                                         const QList<qint64>& vehicleIds,
                                                         QString& errorMessage) const
{
    if (vehicleIds.isEmpty()) {
        return true;
    }

    QStringList idPlaceholders;
    idPlaceholders.reserve(vehicleIds.size());
    for (qsizetype i = 0; i < vehicleIds.size(); ++i) {
        idPlaceholders.append(QStringLiteral("?"));
    }

    const QString vehiclesTable = PostGisSql::qualifiedTable(config, config.vehiclesTable);
    const QString trajectoryDaysTable = PostGisSql::qualifiedTable(config, config.trajectoryDaysTable);
    const QString sql = QStringLiteral(
                            "UPDATE %1 v SET "
                            "day_count = stats.day_count, "
                            "total_point_count = stats.total_point_count, "
                            "first_seen_at = stats.first_seen_at, "
                            "last_seen_at = stats.last_seen_at, "
                            "updated_at = now() "
                            "FROM ( "
                            "  SELECT vehicle_id, "
                            "         COUNT(*)::INT AS day_count, "
                            "         SUM(point_count)::BIGINT AS total_point_count, "
                            "         MIN((trajectory_date + first_ts) AT TIME ZONE 'Asia/Shanghai') AS first_seen_at, "
                            "         MAX((trajectory_date + last_ts) AT TIME ZONE 'Asia/Shanghai') AS last_seen_at "
                            "  FROM %2 "
                            "  WHERE vehicle_id IN (%3) "
                            "  GROUP BY vehicle_id "
                            ") stats "
                            "WHERE v.vehicle_id = stats.vehicle_id")
                            .arg(vehiclesTable, trajectoryDaysTable, idPlaceholders.join(QStringLiteral(", ")));

    QSqlQuery query(db);
    query.prepare(sql);
    for (qint64 vehicleId : vehicleIds) {
        query.addBindValue(vehicleId);
    }

    if (!query.exec()) {
        errorMessage = QStringLiteral("更新车辆汇总信息失败: %1").arg(query.lastError().text());
        return false;
    }

    return true;
}

PostGisDataManager::ImportFileStatus
PostGisDataManager::importSingleFile(const TrajectoryFileInfo& file,
                                            QSqlDatabase& db,
                                            const PostGisDatabaseConfig& config,
                                            QString& errorMessage,
                                            qint64* importedPoints,
                                            qint64* importedVehicleId) const
{
    if (importedPoints != nullptr) {
        *importedPoints = 0;
    }
    if (importedVehicleId != nullptr) {
        *importedVehicleId = -1;
    }

    if (file.hasPeriod && !file.plateNumber.isEmpty()) {
        const qint64 existingDays = countExistingDays(db,
                                                      config,
                                                      file.plateNumber,
                                                      file.periodStart,
                                                      file.periodEnd,
                                                      errorMessage);
        if (existingDays < 0) {
            return PostGisDataManager::ImportFileStatus::Failed;
        }
        if (existingDays > 0) {
            logImportSkipped(file, file.plateNumber, file.periodStart, file.periodEnd, existingDays);
            return PostGisDataManager::ImportFileStatus::Skipped;
        }
    }

    ExcelParserManager parser;
    QList<TrajectoryPoint> records;
    QString loadError;
    if (!parser.loadVehicleRecords(file.filePath, records, loadError)) {
        errorMessage = loadError.isEmpty()
                           ? QStringLiteral("无法读取 Excel 文件: %1").arg(file.fileName)
                           : loadError;
        return PostGisDataManager::ImportFileStatus::Failed;
    }

    if (records.isEmpty()) {
        errorMessage = QStringLiteral("文件中没有有效轨迹数据: %1").arg(file.fileName);
        return PostGisDataManager::ImportFileStatus::Failed;
    }

    QString plateNumber = file.plateNumber;
    QString plateColor;
    QList<TrajectoryPoint> filteredRecords;
    filteredRecords.reserve(records.size());

    for (const TrajectoryPoint& record : records) {
        if (!record.isValid()) {
            continue;
        }
        if (plateNumber.isEmpty()) {
            plateNumber = record.plateNumber;
        }
        if (record.plateNumber != plateNumber) {
            continue;
        }
        filteredRecords.append(record);
        if (plateColor.isEmpty()) {
            plateColor = normalizePlateColor(record);
        }
    }

    if (plateNumber.isEmpty() || filteredRecords.isEmpty()) {
        errorMessage = QStringLiteral("文件中没有有效轨迹数据或车牌号: %1").arg(file.fileName);
        return PostGisDataManager::ImportFileStatus::Failed;
    }

    std::sort(filteredRecords.begin(),
              filteredRecords.end(),
              [](const TrajectoryPoint& left,
                 const TrajectoryPoint& right) {
                  return left.timestamp < right.timestamp;
              });

    QMap<QDate, QList<TrajectoryPoint>> dayBuckets;
    for (const TrajectoryPoint& record : filteredRecords) {
        const QDate calendarDay = PostGisSql::calendarDate(record.timestamp);
        if (!calendarDay.isValid()) {
            continue;
        }
        dayBuckets[calendarDay].append(record);
    }

    if (dayBuckets.isEmpty()) {
        errorMessage = QStringLiteral("文件中没有有效轨迹时间: %1").arg(file.fileName);
        return PostGisDataManager::ImportFileStatus::Failed;
    }

    QDate periodStart = file.hasPeriod ? file.periodStart : dayBuckets.firstKey();
    QDate periodEnd = file.hasPeriod ? file.periodEnd : dayBuckets.lastKey();

    if (!file.hasPeriod) {
        const qint64 existingDays = countExistingDays(db, config, plateNumber, periodStart, periodEnd, errorMessage);
        if (existingDays < 0) {
            return PostGisDataManager::ImportFileStatus::Failed;
        }
        if (existingDays > 0) {
            logImportSkipped(file, plateNumber, periodStart, periodEnd, existingDays);
            return PostGisDataManager::ImportFileStatus::Skipped;
        }
    }

    if (!db.transaction()) {
        errorMessage = QStringLiteral("无法开启数据库事务: %1").arg(db.lastError().text());
        return PostGisDataManager::ImportFileStatus::Failed;
    }

    QSqlQuery tuningQuery(db);
    tuningQuery.exec(QStringLiteral("SET LOCAL synchronous_commit TO OFF"));

    const qint64 vehicleId = upsertVehicle(db, config, plateNumber, plateColor, errorMessage);
    if (vehicleId < 0) {
        db.rollback();
        return PostGisDataManager::ImportFileStatus::Failed;
    }

    QMap<QDate, qint64> trajectoryDayIds;
    if (!insertTrajectoryDays(db, config, vehicleId, dayBuckets, trajectoryDayIds, errorMessage)) {
        db.rollback();
        return PostGisDataManager::ImportFileStatus::Failed;
    }

    qint64 totalImportedPoints = 0;
    for (auto it = dayBuckets.constBegin(); it != dayBuckets.constEnd(); ++it) {
        const QDate trajectoryDate = it.key();
        const QList<TrajectoryPoint>& dayRecords = it.value();
        if (dayRecords.isEmpty()) {
            continue;
        }

        const qint64 trajectoryDayId = trajectoryDayIds.value(trajectoryDate, -1);
        if (trajectoryDayId < 0) {
            errorMessage = QStringLiteral("未找到日航迹 ID: %1").arg(trajectoryDate.toString(Qt::ISODate));
            db.rollback();
            return PostGisDataManager::ImportFileStatus::Failed;
        }

        if (!insertTrajectoryBatch(db, config, dayRecords, trajectoryDayId, errorMessage)) {
            errorMessage = QStringLiteral("写入轨迹点失败 (%1): %2").arg(file.fileName, errorMessage);
            db.rollback();
            return PostGisDataManager::ImportFileStatus::Failed;
        }

        totalImportedPoints += dayRecords.size();
    }

    if (!db.commit()) {
        errorMessage = QStringLiteral("提交数据库事务失败: %1").arg(db.lastError().text());
        db.rollback();
        return PostGisDataManager::ImportFileStatus::Failed;
    }

    if (importedPoints != nullptr) {
        *importedPoints = totalImportedPoints;
    }
    if (importedVehicleId != nullptr) {
        *importedVehicleId = vehicleId;
    }
    AppLogger::info(QStringLiteral("导入成功: 文件=%1 | 车牌=%2 | 时段=%3~%4 | 新增 %5 点")
                        .arg(file.fileName,
                             plateNumber,
                             periodStart.toString(Qt::ISODate),
                             periodEnd.toString(Qt::ISODate))
                        .arg(totalImportedPoints));
    return PostGisDataManager::ImportFileStatus::Imported;
}

bool PostGisDataManager::importFolder(const QString& folderPath,
                                             const PostGisDatabaseConfig& config,
                                             QString& errorMessage,
                                             TrajectoryImportResult* result)
{
    errorMessage.clear();
    TrajectoryImportResult localResult;
    if (result == nullptr) {
        result = &localResult;
    } else {
        *result = TrajectoryImportResult{};
    }

    if (!config.isValid()) {
        errorMessage = QStringLiteral("PostGIS 数据库配置不完整，请先在轨迹面板配置 ini");
        AppLogger::error(errorMessage);
        return false;
    }

    const QList<TrajectoryFileInfo> files = collectTrajectoryFiles(folderPath);
    if (files.isEmpty()) {
        errorMessage = QStringLiteral("文件夹中没有找到轨迹 Excel 文件");
        AppLogger::warn(QStringLiteral("导入终止: 目录=%1 | %2").arg(folderPath, errorMessage));
        return false;
    }

    AppLogger::info(QStringLiteral("开始导入: 目录=%1 | 待处理文件=%2").arg(folderPath).arg(files.size()));

    const QString connectionName = PostGisConnection::makeConnectionName(QStringLiteral("carmove_import"));

    {
        QString connectError;
        if (!PostGisConnection::openDatabase(config, connectionName, connectError)) {
            errorMessage = connectError;
            AppLogger::error(QStringLiteral("导入失败: 无法连接数据库 | %1").arg(errorMessage));
            return false;
        }

        QSqlDatabase db = QSqlDatabase::database(connectionName);

        applyImportSessionSettings(db);

        result->totalFiles = files.size();
        int processed = 0;
        QSet<qint64> vehiclesToRefresh;

        for (const TrajectoryFileInfo& file : files) {
            QString fileError;
            qint64 importedPoints = 0;
            qint64 importedVehicleId = -1;
            const PostGisDataManager::ImportFileStatus status =
                importSingleFile(file, db, config, fileError, &importedPoints, &importedVehicleId);
            if (status == PostGisDataManager::ImportFileStatus::Imported) {
                ++result->importedFiles;
                result->importedPoints += importedPoints;
                if (importedVehicleId >= 0) {
                    vehiclesToRefresh.insert(importedVehicleId);
                }
            } else if (status == PostGisDataManager::ImportFileStatus::Skipped) {
                ++result->skippedFiles;
            } else {
                ++result->failedFiles;
                AppLogger::warn(QStringLiteral("导入失败: 文件=%1 | %2").arg(file.fileName, fileError));
                if (result->errorSamples.size() < 8) {
                    result->errorSamples.append(QStringLiteral("%1: %2").arg(file.fileName, fileError));
                }
            }

            ++processed;
            emit importProgress((processed * 100) / files.size());
            QCoreApplication::processEvents();
        }

        QString statsError;
        if (!refreshVehicleStatsBatch(db,
                                      config,
                                      vehiclesToRefresh.values(),
                                      statsError)) {
            errorMessage = statsError;
            AppLogger::error(QStringLiteral("导入失败: 更新车辆汇总信息 | %1").arg(errorMessage));
            PostGisConnection::closeDatabase(connectionName);
            return false;
        }
    }
    PostGisConnection::closeDatabase(connectionName);

    AppLogger::info(QStringLiteral("导入汇总: 共 %1 文件 | 成功 %2 | 跳过 %3 | 失败 %4 | 新增 %5 点")
                        .arg(result->totalFiles)
                        .arg(result->importedFiles)
                        .arg(result->skippedFiles)
                        .arg(result->failedFiles)
                        .arg(result->importedPoints));

    if (result->importedFiles == 0 && result->skippedFiles == 0) {
        errorMessage = result->errorSamples.isEmpty()
                           ? QStringLiteral("没有成功导入任何轨迹文件")
                           : result->errorSamples.join(QStringLiteral("\n"));
        AppLogger::error(QStringLiteral("导入失败: %1").arg(errorMessage));
        return false;
    }

    return true;
}

