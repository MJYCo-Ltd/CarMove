#ifndef POSTGISDATAMANAGER_H
#define POSTGISDATAMANAGER_H

#include "Domain/TrajectoryTypes.h"
#include "DataManagement/PostGisDatabaseConfig.h"
#include "DataParsing/TrajectoryFileNaming.h"

#include <QDate>
#include <QDateTime>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

struct TrajectoryImportResult {
    int totalFiles = 0;
    int importedFiles = 0;
    int skippedFiles = 0;
    int failedFiles = 0;
    qint64 importedPoints = 0;
    QStringList errorSamples;
};

/**
 * @brief PostGIS 数据管理器：统一车辆目录、日轨迹读取与 Excel 导入写入。
 */
class PostGisDataManager : public QObject
{
    Q_OBJECT

public:
    explicit PostGisDataManager(QObject* parent = nullptr);
    ~PostGisDataManager() override;

    bool isConnected() const;
    PostGisDatabaseConfig config() const;

    bool connectDatabase(const PostGisDatabaseConfig& config, QString& errorMessage);
    void disconnectDatabase();

    QList<VehicleSummary> listVehicles(QString& errorMessage) const;
    QList<TrajectoryPoint> loadTrajectoryPoints(const QString& plateNumber,
                                                QString& errorMessage,
                                                const QDateTime& startDateTime = {},
                                                const QDateTime& endDateTime = {}) const;
    QList<VehicleDayTrajectory> loadTrajectoryByDay(const QString& plateNumber,
                                                      QString& errorMessage,
                                                      const QDate& startDate = {},
                                                      const QDate& endDate = {}) const;

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
                                      class QSqlDatabase& db,
                                      const PostGisDatabaseConfig& config,
                                      QString& errorMessage,
                                      qint64* importedPoints,
                                      qint64* importedVehicleId) const;
    void applyImportSessionSettings(class QSqlDatabase& db) const;
    qint64 countExistingDays(class QSqlDatabase& db,
                             const PostGisDatabaseConfig& config,
                             const QString& plateNumber,
                             const QDate& periodStart,
                             const QDate& periodEnd,
                             QString& errorMessage) const;
    bool insertTrajectoryDays(class QSqlDatabase& db,
                                const PostGisDatabaseConfig& config,
                                qint64 vehicleId,
                                const QMap<QDate, QList<TrajectoryPoint>>& dayBuckets,
                                QMap<QDate, qint64>& trajectoryDayIds,
                                QString& errorMessage) const;
    bool insertTrajectoryBatch(class QSqlDatabase& db,
                               const PostGisDatabaseConfig& config,
                               const QList<TrajectoryPoint>& records,
                               qint64 trajectoryDayId,
                               QString& errorMessage) const;
    qint64 upsertVehicle(class QSqlDatabase& db,
                         const PostGisDatabaseConfig& config,
                         const QString& plateNumber,
                         const QString& plateColor,
                         QString& errorMessage) const;
    bool refreshVehicleStatsBatch(class QSqlDatabase& db,
                                  const PostGisDatabaseConfig& config,
                                  const QList<qint64>& vehicleIds,
                                  QString& errorMessage) const;

    PostGisDatabaseConfig m_config;
    QString m_connectionName;
    bool m_connected = false;
};

#endif // POSTGISDATAMANAGER_H
