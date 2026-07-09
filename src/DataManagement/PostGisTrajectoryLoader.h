#pragma once

#include "DataManagement/FolderScanner.h"
#include "DataParsing/ExcelDataReader.h"
#include "DataManagement/PostGisDatabaseConfig.h"

#include <QDate>
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
                                                         QString& errorMessage,
                                                         const QDate& startDate = {},
                                                         const QDate& endDate = {}) const;

private:
    PostGisDatabaseConfig m_config;
    QString m_connectionName;
    bool m_connected = false;
};
