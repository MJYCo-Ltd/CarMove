#include "DataManagement/VehicleManager.h"
#include "Map/CoordinateConverter.h"
#include "Core/AppLogger.h"

#include <algorithm>

VehicleManager::VehicleManager(QObject *parent)
    : QObject(parent)
{
}

void VehicleManager::setDataManager(TrajectoryDataManager* manager)
{
    if (m_dataManager == manager) {
        return;
    }

    if (m_dataManager) {
        disconnect(m_dataManager, nullptr, this, nullptr);
    }

    m_dataManager = manager;
    if (!m_dataManager) {
        return;
    }

    connect(m_dataManager, &TrajectoryDataManager::loadProgress,
            this, &VehicleManager::loadingProgress);
}

void VehicleManager::setVehicleList(const QList<TrajectoryDataManager::VehicleInfo>& vehicles)
{
    m_vehicleList = vehicles;
    
    if (!m_selectedVehicle.isEmpty()) {
        bool found = false;
        for (const TrajectoryDataManager::VehicleInfo& vehicleInfo : vehicles) {
            if (vehicleInfo.plateNumber == m_selectedVehicle) {
                found = true;
                break;
            }
        }
        if (!found) {
            m_selectedVehicle.clear();
            m_currentTrajectory.clear();
            m_convertedTrajectory.clear();
        }
    }
}

void VehicleManager::selectVehicle(const QString& plateNumber,
                                   const QDateTime& startDateTime,
                                   const QDateTime& endDateTime)
{
    m_selectedVehicle = plateNumber;
    m_currentTrajectory.clear();
    m_convertedTrajectory.clear();

    emit vehicleSelected(plateNumber);
    loadTrajectory(plateNumber, startDateTime, endDateTime);
}

void VehicleManager::loadTrajectory(const QString& plateNumber,
                                    const QDateTime& startDateTime,
                                    const QDateTime& endDateTime,
                                    bool preserveAllPoints)
{
    if (plateNumber.isEmpty()) {
        AppLogger::warn(QStringLiteral("无法加载轨迹: 车牌号为空"));
        emit trajectoryLoaded(plateNumber, {});
        return;
    }

    if (!m_dataManager) {
        AppLogger::warn(QStringLiteral("无法加载轨迹: 数据源管理器未初始化"));
        emit trajectoryLoaded(plateNumber, {});
        return;
    }

    m_currentTrajectory.clear();
    m_selectedVehicle = plateNumber;

    TrajectoryDataManager::TrajectoryLoadRequest request;
    request.plateNumber = plateNumber;
    request.startDateTime = startDateTime;
    request.endDateTime = endDateTime;
    request.preserveAllPoints = preserveAllPoints;

    const TrajectoryDataManager::TrajectoryLoadResult result = m_dataManager->loadTrajectory(request);
    if (!result.success) {
        if (!result.errorMessage.isEmpty()) {
            AppLogger::warn(QStringLiteral("加载轨迹失败: 车牌=%1 | %2")
                                .arg(plateNumber, result.errorMessage));
        }
        emit trajectoryLoaded(plateNumber, {});
        return;
    }

    finalizeLoadedTrajectory(result.records, preserveAllPoints);
}

void VehicleManager::finalizeLoadedTrajectory(const QList<TrajectoryPoint>& allRecords,
                                              bool preserveAllPoints)
{
    QList<TrajectoryPoint> sortedRecords = allRecords;
    std::sort(sortedRecords.begin(), sortedRecords.end(),
              [](const TrajectoryPoint& a, const TrajectoryPoint& b) {
                  return a.timestamp < b.timestamp;
              });
    
    QList<TrajectoryPoint> filteredRecords;
    if (preserveAllPoints) {
        filteredRecords = sortedRecords;
    } else if (!sortedRecords.isEmpty()) {
        filteredRecords.reserve(sortedRecords.size());
        filteredRecords.append(sortedRecords.first());
        
        for (int i = 1; i < sortedRecords.size(); ++i) {
            const TrajectoryPoint& currentRecord = sortedRecords.at(i);
            const TrajectoryPoint& previousRecord = filteredRecords.last();
            
            const bool isStationary = (currentRecord.speed == 0.0)
                                      && (currentRecord.totalMileage == previousRecord.totalMileage)
                                      && (!currentRecord.totalMileage.isEmpty());
            
            if (!isStationary) {
                filteredRecords.append(currentRecord);
            }
        }
    }
    
    m_currentTrajectory = filteredRecords;
    
    if (m_coordinateConversionEnabled) {
        applyCoordinateConversionToCurrentTrajectory();
    } else {
        m_convertedTrajectory = m_currentTrajectory;
    }
    
    emit trajectoryLoaded(m_selectedVehicle, m_convertedTrajectory);
    emit loadingProgress(100);
}

void VehicleManager::applyCoordinateConversion(bool enabled)
{
    m_coordinateConversionEnabled = enabled;
    
    if (!m_currentTrajectory.isEmpty()) {
        applyCoordinateConversionToCurrentTrajectory();
        emit trajectoryConverted(m_selectedVehicle, m_convertedTrajectory);
    }
}

void VehicleManager::applyCoordinateConversionToCurrentTrajectory()
{
    m_convertedTrajectory.clear();
    
    if (m_currentTrajectory.isEmpty()) {
        return;
    }
    
    for (const TrajectoryPoint& record : m_currentTrajectory) {
        TrajectoryPoint convertedRecord = record;
        
        if (m_coordinateConversionEnabled) {
            const QGeoCoordinate originalCoord(record.latitude, record.longitude);
            const QGeoCoordinate convertedCoord = CoordinateConverter::wgs84ToGcj02(originalCoord);
            
            convertedRecord.latitude = convertedCoord.latitude();
            convertedRecord.longitude = convertedCoord.longitude();
        }
        
        m_convertedTrajectory.append(convertedRecord);
    }
}

QList<TrajectoryPoint> VehicleManager::getCurrentTrajectory() const
{
    return m_currentTrajectory;
}

QList<TrajectoryPoint> VehicleManager::getConvertedTrajectory() const
{
    return m_convertedTrajectory;
}

QString VehicleManager::getSelectedVehicle() const
{
    return m_selectedVehicle;
}
