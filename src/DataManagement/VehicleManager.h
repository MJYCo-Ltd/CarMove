#ifndef VEHICLEMANAGER_H
#define VEHICLEMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QDate>
#include "DataManagement/TrajectoryDataManager.h"
#include "Domain/TrajectoryTypes.h"

class VehicleManager : public QObject
{
    Q_OBJECT
    
public:
    explicit VehicleManager(QObject *parent = nullptr);
    
    void setDataManager(TrajectoryDataManager* manager);
    void setVehicleList(const QList<TrajectoryDataManager::VehicleInfo>& vehicles);
    void selectVehicle(const QString& plateNumber);
    void loadTrajectory(const QString& plateNumber,
                        const QDate& startDate = {},
                        const QDate& endDate = {},
                        bool preserveAllPoints = false);
    void applyCoordinateConversion(bool enabled);
    QList<TrajectoryPoint> getCurrentTrajectory() const;
    QList<TrajectoryPoint> getConvertedTrajectory() const;
    const QList<TrajectoryPoint>& currentTrajectoryRef() const { return m_currentTrajectory; }
    const QList<TrajectoryPoint>& convertedTrajectoryRef() const { return m_convertedTrajectory; }
    QString getSelectedVehicle() const;
    
signals:
    void vehicleSelected(const QString& plateNumber);
    void trajectoryLoaded(const QString& plateNumber, 
                         const QList<TrajectoryPoint>& trajectory);
    void trajectoryConverted(const QString& plateNumber,
                           const QList<TrajectoryPoint>& convertedTrajectory);
    void loadingProgress(int percentage);
    
private:
    void finalizeLoadedTrajectory(const QList<TrajectoryPoint>& allRecords,
                                  bool preserveAllPoints = false);
    void applyCoordinateConversionToCurrentTrajectory();

    TrajectoryDataManager* m_dataManager = nullptr;
    QList<TrajectoryDataManager::VehicleInfo> m_vehicleList;
    QString m_selectedVehicle;
    QList<TrajectoryPoint> m_currentTrajectory;
    QList<TrajectoryPoint> m_convertedTrajectory;
    bool m_coordinateConversionEnabled = false;
};

#endif // VEHICLEMANAGER_H
