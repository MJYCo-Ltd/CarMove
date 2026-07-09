#pragma once

#include "PostGisDatabaseConfig.h"
#include "ParseData/ExcelDataReader.h"
#include "ParseData/TrajectoryFileNaming.h"

#include <QDate>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTime>

#include <QSqlDatabase>

struct TrajectoryImportResult {
    int totalFiles = 0;
    int importedFiles = 0;
    int skippedFiles = 0;
    int failedFiles = 0;
    qint64 importedPoints = 0;
    QStringList errorSamples;
};

class PostGisTrajectoryImporter : public QObject
{
    Q_OBJECT

public:
    explicit PostGisTrajectoryImporter(QObject* parent = nullptr);

    bool importFolder(const QString& folderPath,
                      const PostGisDatabaseConfig& config,
                      QString& errorMessage,
                      TrajectoryImportResult* result = nullptr);

signals:
    void importProgress(int percentage);

private:
    enum class ImportFileStatus {
        Imported,
        Skipped,
        Failed
    };

    QList<TrajectoryFileInfo> collectTrajectoryFiles(const QString& folderPath) const;

    ImportFileStatus importSingleFile(const TrajectoryFileInfo& file,
                                      QSqlDatabase& db,
                                      const PostGisDatabaseConfig& config,
                                      QString& errorMessage,
                                      qint64* importedPoints,
                                      qint64* importedVehicleId = nullptr) const;

    void applyImportSessionSettings(QSqlDatabase& db) const;

    qint64 countExistingDays(QSqlDatabase& db,
                             const PostGisDatabaseConfig& config,
                             const QString& plateNumber,
                             const QDate& periodStart,
                             const QDate& periodEnd,
                             QString& errorMessage) const;

    bool insertTrajectoryDays(QSqlDatabase& db,
                              const PostGisDatabaseConfig& config,
                              qint64 vehicleId,
                              const QMap<QDate, QList<ExcelDataReader::VehicleRecord>>& dayBuckets,
                              QMap<QDate, qint64>& trajectoryDayIds,
                              QString& errorMessage) const;

    bool insertTrajectoryBatch(QSqlDatabase& db,
                               const PostGisDatabaseConfig& config,
                               const QList<ExcelDataReader::VehicleRecord>& records,
                               qint64 trajectoryDayId,
                               QString& errorMessage) const;

    qint64 upsertVehicle(QSqlDatabase& db,
                         const PostGisDatabaseConfig& config,
                         const QString& plateNumber,
                         const QString& plateColor,
                         QString& errorMessage) const;

    bool refreshVehicleStatsBatch(QSqlDatabase& db,
                                  const PostGisDatabaseConfig& config,
                                  const QList<qint64>& vehicleIds,
                                  QString& errorMessage) const;
};
