#pragma once

#include "PostGisDatabaseConfig.h"
#include "ParseData/ExcelDataReader.h"

#include <QDate>
#include <QObject>
#include <QString>
#include <QStringList>

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

    struct ParsedTrajectoryFile {
        QString filePath;
        QString fileName;
        QString plateNumber;
        QDate periodStart;
        QDate periodEnd;
        bool hasPeriod = false;
    };

    bool ensureDriverAvailable(QString& errorMessage) const;
    bool validateIdentifier(const QString& identifier, QString& errorMessage) const;
    QString quotedIdentifier(const QString& identifier) const;
    QString qualifiedTable(const PostGisDatabaseConfig& config, const QString& tableName) const;

    QList<ParsedTrajectoryFile> collectTrajectoryFiles(const QString& folderPath) const;
    ParsedTrajectoryFile parseFileName(const QString& filePath) const;

    ImportFileStatus importSingleFile(const ParsedTrajectoryFile& file,
                                      QSqlDatabase& db,
                                      const PostGisDatabaseConfig& config,
                                      QString& errorMessage,
                                      qint64* importedPoints) const;

    qint64 countExistingPoints(QSqlDatabase& db,
                               const PostGisDatabaseConfig& config,
                               const QString& plateNumber,
                               const QDate& periodStart,
                               const QDate& periodEnd,
                               QString& errorMessage) const;
    bool insertTrajectoryBatch(QSqlDatabase& db,
                               const PostGisDatabaseConfig& config,
                               const QList<ExcelDataReader::VehicleRecord>& records,
                               qint64 vehicleId,
                               const QString& plateNumber,
                               QString& errorMessage) const;

    qint64 upsertVehicle(QSqlDatabase& db,
                         const PostGisDatabaseConfig& config,
                         const QString& plateNumber,
                         const QString& plateColor,
                         QString& errorMessage) const;
};
