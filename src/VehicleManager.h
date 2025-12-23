#ifndef VEHICLEMANAGER_H
#define VEHICLEMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include "FolderScanner.h"
#include "ExcelDataReader.h"

class VehicleManager : public QObject
{
    Q_OBJECT
    
public:
    explicit VehicleManager(QObject *parent = nullptr);
    
    void setVehicleList(const QList<FolderScanner::VehicleInfo>& vehicles);
    void selectVehicle(const QString& plateNumber);
    void loadVehicleTrajectory(const QString& plateNumber);
    void applyCoordinateConversion(bool enabled);
    QList<ExcelDataReader::VehicleRecord> getCurrentTrajectory() const;
    QList<ExcelDataReader::VehicleRecord> getConvertedTrajectory() const;
    
    // Additional utility methods
    QString getSelectedVehicle() const;
    bool isCoordinateConversionEnabled() const;
    QStringList getAvailableVehicles() const;
    bool hasTrajectoryData() const;
    
signals:
    void vehicleSelected(const QString& plateNumber);
    void trajectoryLoaded(const QString& plateNumber, 
                         const QList<ExcelDataReader::VehicleRecord>& trajectory);
    void trajectoryConverted(const QString& plateNumber,
                           const QList<ExcelDataReader::VehicleRecord>& convertedTrajectory);
    void loadingProgress(int percentage);
    
private:
    QList<FolderScanner::VehicleInfo> m_vehicleList;
    QString m_selectedVehicle;
    QList<ExcelDataReader::VehicleRecord> m_currentTrajectory;
    QList<ExcelDataReader::VehicleRecord> m_convertedTrajectory;
    bool m_coordinateConversionEnabled;
    
    ExcelDataReader* m_excelReader;
    
    // Helper method to apply coordinate conversion to current trajectory
    void applyCoordinateConversionToCurrentTrajectory();
};

#endif // VEHICLEMANAGER_H