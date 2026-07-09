#ifndef VEHICLEMANAGER_H
#define VEHICLEMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QDate>
#include "DataManagement/FolderScanner.h"
#include "DataParsing/ExcelDataReader.h"

class PostGisTrajectoryLoader;

class VehicleManager : public QObject
{
    Q_OBJECT
    
public:
    explicit VehicleManager(QObject *parent = nullptr);
    
    void setVehicleList(const QList<FolderScanner::VehicleInfo>& vehicles);
    void setDatabaseMode(bool enabled);
    void setPostGisLoader(PostGisTrajectoryLoader* loader);
    void selectVehicle(const QString& plateNumber);
    void loadVehicleTrajectory(const QString& plateNumber);
    void loadTrajectoryFromDatabase(const QString& plateNumber,
                                    const QDate& startDate = {},
                                    const QDate& endDate = {},
                                    bool preserveAllPoints = false);
    void applyCoordinateConversion(bool enabled);
    QList<ExcelDataReader::VehicleRecord> getCurrentTrajectory() const;
    QList<ExcelDataReader::VehicleRecord> getConvertedTrajectory() const;
    
    // Additional utility methods
    QString getSelectedVehicle() const;
    
signals:
    void vehicleSelected(const QString& plateNumber);
    void trajectoryLoaded(const QString& plateNumber, 
                         const QList<ExcelDataReader::VehicleRecord>& trajectory);
    void trajectoryConverted(const QString& plateNumber,
                           const QList<ExcelDataReader::VehicleRecord>& convertedTrajectory);
    void loadingProgress(int percentage);
    
private:
    void finalizeLoadedTrajectory(const QList<ExcelDataReader::VehicleRecord>& allRecords,
                                  bool preserveAllPoints = false);
    void loadVehicleTrajectoryFromDatabase(const QString& plateNumber);

    QList<FolderScanner::VehicleInfo> m_vehicleList;
    QString m_selectedVehicle;
    QList<ExcelDataReader::VehicleRecord> m_currentTrajectory;
    QList<ExcelDataReader::VehicleRecord> m_convertedTrajectory;
    bool m_coordinateConversionEnabled;
    bool m_databaseMode = false;
    
    ExcelDataReader* m_excelReader;
    PostGisTrajectoryLoader* m_postGisLoader = nullptr;
    
    // Helper method to apply coordinate conversion to current trajectory
    void applyCoordinateConversionToCurrentTrajectory();
};

#endif // VEHICLEMANAGER_H
