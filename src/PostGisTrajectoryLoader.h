#pragma once

#include "FolderScanner.h"
#include "ParseData/ExcelDataReader.h"
#include "PostGisDatabaseConfig.h"

#include <QObject>
#include <QSqlDatabase>

class PostGisTrajectoryLoader : public QObject
{
    Q_OBJECT

public:
    explicit PostGisTrajectoryLoader(QObject* parent = nullptr);
    ~PostGisTrajectoryLoader() override;

    bool isConnected() const { return m_connected; }
    PostGisDatabaseConfig config() const { return m_config; }

    bool connectDatabase(const PostGisDatabaseConfig& config, QString& errorMessage);
    void disconnectDatabase();

    QList<FolderScanner::VehicleInfo> listVehicles(QString& errorMessage) const;
    QList<ExcelDataReader::VehicleRecord> loadTrajectory(const QString& plateNumber,
                                                         QString& errorMessage) const;

private:
    QString qualifiedTable(const QString& tableName) const;
    QString quotedIdentifier(const QString& identifier) const;
    bool validateIdentifier(const QString& identifier, QString& errorMessage) const;
    bool ensureDriverAvailable(QString& errorMessage) const;

    PostGisDatabaseConfig m_config;
    QString m_connectionName;
    bool m_connected = false;
};
