#include "PostGisTrajectoryImporter.h"

#include "ParseData/ExcelDataReader.h"
#include "ParseData/ExcelFilePath.h"

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <algorithm>

namespace {

QString normalizePlateColor(const ExcelDataReader::VehicleRecord& record)
{
    if (record.vehicleColor == QStringLiteral("yellow") || record.vehicleColor == QStringLiteral("blue")) {
        return record.vehicleColor;
    }
    if (record.vehicleColor.contains(QStringLiteral("黄"), Qt::CaseInsensitive)) {
        return QStringLiteral("yellow");
    }
    return record.vehicleColor.isEmpty() ? QString() : record.vehicleColor;
}

} // namespace

PostGisTrajectoryImporter::PostGisTrajectoryImporter(QObject* parent)
    : QObject(parent)
{
}

bool PostGisTrajectoryImporter::ensureDriverAvailable(QString& errorMessage) const
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QPSQL"))) {
        errorMessage = QStringLiteral("未找到 PostgreSQL 驱动 (QPSQL)，请确认已安装 Qt SQL 驱动 libpq 插件");
        return false;
    }
    return true;
}

bool PostGisTrajectoryImporter::validateIdentifier(const QString& identifier, QString& errorMessage) const
{
    static const QRegularExpression identifierPattern(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    if (!identifierPattern.match(identifier).hasMatch()) {
        errorMessage = QStringLiteral("非法 SQL 标识符: %1").arg(identifier);
        return false;
    }
    return true;
}

QString PostGisTrajectoryImporter::quotedIdentifier(const QString& identifier) const
{
    return QLatin1Char('"') + identifier + QLatin1Char('"');
}

QString PostGisTrajectoryImporter::qualifiedTable(const PostGisDatabaseConfig& config,
                                                  const QString& tableName) const
{
    return quotedIdentifier(config.schema) + QLatin1Char('.') + quotedIdentifier(tableName);
}

PostGisTrajectoryImporter::ParsedTrajectoryFile
PostGisTrajectoryImporter::parseFileName(const QString& filePath) const
{
    ParsedTrajectoryFile parsed;
    parsed.filePath = filePath;
    parsed.fileName = QFileInfo(filePath).fileName();

    static const QRegularExpression rangePattern(
        u8"^([京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼][A-Z][A-Z0-9]{5,6})"
        u8"-(\\d{4}-\\d{2}-\\d{2})-(\\d{4}-\\d{2}-\\d{2})\\.(xlsx|xls)$",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression singleDatePattern(
        u8"^([京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼][A-Z][A-Z0-9]{5,6})"
        u8"-(\\d{4}-\\d{2}-\\d{2})\\.(xlsx|xls)$",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression plateOnlyPattern(
        u8"^([京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼][A-Z][A-Z0-9]{5,6})\\.(xlsx|xls)$",
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch match = rangePattern.match(parsed.fileName);
    if (match.hasMatch()) {
        parsed.plateNumber = match.captured(1);
        parsed.periodStart = QDate::fromString(match.captured(2), QStringLiteral("yyyy-MM-dd"));
        parsed.periodEnd = QDate::fromString(match.captured(3), QStringLiteral("yyyy-MM-dd"));
        parsed.hasPeriod = parsed.periodStart.isValid() && parsed.periodEnd.isValid();
        return parsed;
    }

    match = singleDatePattern.match(parsed.fileName);
    if (match.hasMatch()) {
        parsed.plateNumber = match.captured(1);
        parsed.periodStart = QDate::fromString(match.captured(2), QStringLiteral("yyyy-MM-dd"));
        parsed.periodEnd = parsed.periodStart;
        parsed.hasPeriod = parsed.periodStart.isValid();
        return parsed;
    }

    match = plateOnlyPattern.match(parsed.fileName);
    if (match.hasMatch()) {
        parsed.plateNumber = match.captured(1);
    }

    return parsed;
}

QList<PostGisTrajectoryImporter::ParsedTrajectoryFile>
PostGisTrajectoryImporter::collectTrajectoryFiles(const QString& folderPath) const
{
    QList<ParsedTrajectoryFile> files;
    const QString localPath = ExcelFilePath::normalizeLocalFilePath(folderPath);
    if (localPath.isEmpty()) {
        return files;
    }

    QDirIterator iterator(localPath,
                          QStringList{QStringLiteral("*.xlsx"),
                                      QStringLiteral("*.xls"),
                                      QStringLiteral("*.XLSX"),
                                      QStringLiteral("*.XLS")},
                          QDir::Files,
                          QDirIterator::Subdirectories);

    while (iterator.hasNext()) {
        const QString filePath = iterator.next();
        const QFileInfo fileInfo(filePath);
        if (fileInfo.size() == 0 || fileInfo.size() > 500LL * 1024 * 1024) {
            continue;
        }

        files.append(parseFileName(filePath));
    }

    return files;
}

qint64 PostGisTrajectoryImporter::countExistingPoints(QSqlDatabase& db,
                                                      const PostGisDatabaseConfig& config,
                                                      const QString& plateNumber,
                                                      const QDate& periodStart,
                                                      const QDate& periodEnd,
                                                      QString& errorMessage) const
{
    if (!periodStart.isValid() || !periodEnd.isValid()) {
        return -1;
    }

    const QString trajectoryTable = qualifiedTable(config, config.trajectoryTable);
    const QString sql = QStringLiteral(
                            "SELECT COUNT(*) FROM %1 WHERE %2 = :plate "
                            "AND %3 >= :start_ts AND %3 < :end_ts")
                            .arg(trajectoryTable,
                                 quotedIdentifier(config.plateColumn),
                                 quotedIdentifier(config.timeColumn));

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(QStringLiteral(":plate"), plateNumber);
    query.bindValue(QStringLiteral(":start_ts"), periodStart.startOfDay());
    query.bindValue(QStringLiteral(":end_ts"), periodEnd.addDays(1).startOfDay());

    if (!query.exec() || !query.next()) {
        errorMessage = QStringLiteral("检查已有轨迹失败: %1").arg(query.lastError().text());
        return -1;
    }

    return query.value(0).toLongLong();
}

bool PostGisTrajectoryImporter::insertTrajectoryBatch(QSqlDatabase& db,
                                                      const PostGisDatabaseConfig& config,
                                                      const QList<ExcelDataReader::VehicleRecord>& records,
                                                      qint64 vehicleId,
                                                      const QString& plateNumber,
                                                      QString& errorMessage) const
{
    static constexpr qsizetype batchSize = 1000;

    const QString trajectoryTable = qualifiedTable(config, config.trajectoryTable);
    const QString insertPrefix = QStringLiteral(
                                     "INSERT INTO %1 (%2, %3, %4, %5, %6, %7, %8) VALUES ")
                                     .arg(trajectoryTable,
                                          quotedIdentifier(QStringLiteral("vehicle_id")),
                                          quotedIdentifier(config.plateColumn),
                                          quotedIdentifier(config.timeColumn),
                                          quotedIdentifier(config.geomColumn),
                                          quotedIdentifier(config.speedColumn),
                                          quotedIdentifier(config.directionColumn),
                                          quotedIdentifier(config.mileageColumn));

    for (qsizetype offset = 0; offset < records.size(); offset += batchSize) {
        const qsizetype currentBatchSize = std::min(batchSize, records.size() - offset);
        QStringList placeholders;
        placeholders.reserve(currentBatchSize);
        for (qsizetype i = 0; i < currentBatchSize; ++i) {
            placeholders.append(QStringLiteral("(?, ?, ?, ST_SetSRID(ST_MakePoint(?, ?), 4326), ?, ?, ?)"));
        }

        QSqlQuery query(db);
        query.prepare(insertPrefix + placeholders.join(QStringLiteral(", ")));

        for (qsizetype i = 0; i < currentBatchSize; ++i) {
            const ExcelDataReader::VehicleRecord& record = records.at(offset + i);
            query.addBindValue(vehicleId);
            query.addBindValue(plateNumber);
            query.addBindValue(record.timestamp);
            query.addBindValue(record.longitude);
            query.addBindValue(record.latitude);
            query.addBindValue(record.speed);
            query.addBindValue(record.direction);
            query.addBindValue(record.totalMileage);
        }

        if (!query.exec()) {
            errorMessage = QStringLiteral("批量写入轨迹点失败: %1").arg(query.lastError().text());
            return false;
        }

        QCoreApplication::processEvents();
    }

    return true;
}
qint64 PostGisTrajectoryImporter::upsertVehicle(QSqlDatabase& db,
                                                const PostGisDatabaseConfig& config,
                                                const QString& plateNumber,
                                                const QString& plateColor,
                                                QString& errorMessage) const
{
    const QString vehiclesTable = qualifiedTable(config, config.vehiclesTable);
    const QString sql = QStringLiteral(
                            "INSERT INTO %1 (%2, %3) VALUES (:plate, :color) "
                            "ON CONFLICT (%2) DO UPDATE SET "
                            "%3 = COALESCE(EXCLUDED.%3, %1.%3), updated_at = now() "
                            "RETURNING vehicle_id")
                            .arg(vehiclesTable,
                                 quotedIdentifier(QStringLiteral("plate_number")),
                                 quotedIdentifier(QStringLiteral("plate_color")));

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

PostGisTrajectoryImporter::ImportFileStatus
PostGisTrajectoryImporter::importSingleFile(const ParsedTrajectoryFile& file,
                                            QSqlDatabase& db,
                                            const PostGisDatabaseConfig& config,
                                            QString& errorMessage,
                                            qint64* importedPoints) const
{
    if (importedPoints != nullptr) {
        *importedPoints = 0;
    }

    if (file.hasPeriod && !file.plateNumber.isEmpty()) {
        const qint64 existingPoints = countExistingPoints(db,
                                                          config,
                                                          file.plateNumber,
                                                          file.periodStart,
                                                          file.periodEnd,
                                                          errorMessage);
        if (existingPoints < 0) {
            return ImportFileStatus::Failed;
        }
        if (existingPoints > 0) {
            return ImportFileStatus::Skipped;
        }
    }

    ExcelDataReader reader;
    if (!reader.loadExcelFile(file.filePath)) {
        errorMessage = QStringLiteral("无法读取 Excel 文件: %1").arg(file.fileName);
        return ImportFileStatus::Failed;
    }

    QList<ExcelDataReader::VehicleRecord> records = reader.getVehicleData();
    if (records.isEmpty()) {
        errorMessage = QStringLiteral("文件中没有有效轨迹数据: %1").arg(file.fileName);
        return ImportFileStatus::Failed;
    }

    QString plateNumber = file.plateNumber;
    QString plateColor;
    QList<ExcelDataReader::VehicleRecord> filteredRecords;
    filteredRecords.reserve(records.size());

    for (const ExcelDataReader::VehicleRecord& record : records) {
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
        return ImportFileStatus::Failed;
    }

    std::sort(filteredRecords.begin(),
              filteredRecords.end(),
              [](const ExcelDataReader::VehicleRecord& left,
                 const ExcelDataReader::VehicleRecord& right) {
                  return left.timestamp < right.timestamp;
              });

    QDate periodStart = file.periodStart;
    QDate periodEnd = file.periodEnd;
    if (!file.hasPeriod) {
        periodStart = filteredRecords.first().timestamp.date();
        periodEnd = filteredRecords.last().timestamp.date();
    }

    const qint64 existingPoints = countExistingPoints(db, config, plateNumber, periodStart, periodEnd, errorMessage);
    if (existingPoints < 0) {
        return ImportFileStatus::Failed;
    }
    if (existingPoints > 0) {
        return ImportFileStatus::Skipped;
    }

    if (!db.transaction()) {
        errorMessage = QStringLiteral("无法开启数据库事务: %1").arg(db.lastError().text());
        return ImportFileStatus::Failed;
    }

    QSqlQuery tuningQuery(db);
    tuningQuery.exec(QStringLiteral("SET LOCAL synchronous_commit TO OFF"));

    const qint64 vehicleId = upsertVehicle(db, config, plateNumber, plateColor, errorMessage);
    if (vehicleId < 0) {
        db.rollback();
        return ImportFileStatus::Failed;
    }

    if (!insertTrajectoryBatch(db, config, filteredRecords, vehicleId, plateNumber, errorMessage)) {
        errorMessage = QStringLiteral("写入轨迹点失败 (%1): %2").arg(file.fileName, errorMessage);
        db.rollback();
        return ImportFileStatus::Failed;
    }

    if (!db.commit()) {
        errorMessage = QStringLiteral("提交数据库事务失败: %1").arg(db.lastError().text());
        db.rollback();
        return ImportFileStatus::Failed;
    }

    if (importedPoints != nullptr) {
        *importedPoints = filteredRecords.size();
    }
    return ImportFileStatus::Imported;
}
bool PostGisTrajectoryImporter::importFolder(const QString& folderPath,
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
        return false;
    }

    if (!ensureDriverAvailable(errorMessage)) {
        return false;
    }

    const QStringList identifiers = {
        config.schema,
        config.trajectoryTable,
        config.vehiclesTable,
        config.plateColumn,
        config.timeColumn,
        config.geomColumn,
        config.speedColumn,
        config.directionColumn,
        config.mileageColumn,
        config.colorColumn,
    };
    for (const QString& identifier : identifiers) {
        if (!validateIdentifier(identifier, errorMessage)) {
            return false;
        }
    }

    const QList<ParsedTrajectoryFile> files = collectTrajectoryFiles(folderPath);
    if (files.isEmpty()) {
        errorMessage = QStringLiteral("文件夹中没有找到轨迹 Excel 文件");
        return false;
    }

    const QString connectionName =
        QStringLiteral("carmove_import_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), connectionName);
        db.setHostName(config.host);
        db.setPort(config.port);
        db.setDatabaseName(config.database);
        db.setUserName(config.username);
        db.setPassword(config.password);

        if (!db.open()) {
            errorMessage = QStringLiteral("连接数据库失败: %1").arg(db.lastError().text());
            QSqlDatabase::removeDatabase(connectionName);
            return false;
        }

        result->totalFiles = files.size();
        int processed = 0;

        for (const ParsedTrajectoryFile& file : files) {
            QString fileError;
            qint64 importedPoints = 0;
            const ImportFileStatus status = importSingleFile(file, db, config, fileError, &importedPoints);
            if (status == ImportFileStatus::Imported) {
                ++result->importedFiles;
                result->importedPoints += importedPoints;
            } else if (status == ImportFileStatus::Skipped) {
                ++result->skippedFiles;
            } else {
                ++result->failedFiles;
                if (result->errorSamples.size() < 8) {
                    result->errorSamples.append(QStringLiteral("%1: %2").arg(file.fileName, fileError));
                }
            }

            ++processed;
            emit importProgress((processed * 100) / files.size());
            QCoreApplication::processEvents();
        }

        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    if (result->importedFiles == 0 && result->skippedFiles == 0) {
        errorMessage = result->errorSamples.isEmpty()
                           ? QStringLiteral("没有成功导入任何轨迹文件")
                           : result->errorSamples.join(QStringLiteral("\n"));
        return false;
    }

    return true;
}












