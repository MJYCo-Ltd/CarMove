#include "DataManagement/VehicleManager.h"
#include "Map/CoordinateConverter.h"
#include "DataManagement/PostGisTrajectoryLoader.h"
#include "Core/AppLogger.h"

#include <QDate>

VehicleManager::VehicleManager(QObject *parent)
    : QObject(parent)
    , m_coordinateConversionEnabled(false)
    , m_excelReader(new ExcelDataReader(this))
{
}

void VehicleManager::setDatabaseMode(bool enabled)
{
    m_databaseMode = enabled;
}

void VehicleManager::setPostGisLoader(PostGisTrajectoryLoader* loader)
{
    m_postGisLoader = loader;
}

void VehicleManager::setVehicleList(const QList<FolderScanner::VehicleInfo>& vehicles)
{
    m_vehicleList = vehicles;
    
    // Clear current selection if the vehicle is no longer in the list
    if (!m_selectedVehicle.isEmpty()) {
        bool found = false;
        for (const auto& vehicleInfo : vehicles) {
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

void VehicleManager::selectVehicle(const QString& plateNumber)
{
    m_selectedVehicle = plateNumber;

    // Clear previous trajectory data
    m_currentTrajectory.clear();
    m_convertedTrajectory.clear();

    emit vehicleSelected(plateNumber);

    // Load trajectory data for the selected vehicle
    loadVehicleTrajectory(plateNumber);
}

void VehicleManager::loadVehicleTrajectory(const QString& plateNumber)
{
    if (plateNumber.isEmpty()) {
        AppLogger::warn(QStringLiteral("无法加载轨迹: 车牌号为空"));
        return;
    }

    if (m_databaseMode) {
        loadVehicleTrajectoryFromDatabase(plateNumber);
        return;
    }
    
    // Find all file paths for this vehicle
    QStringList filePaths;
    for (const auto& vehicleInfo : m_vehicleList) {
        if (vehicleInfo.plateNumber == plateNumber) {
            filePaths = vehicleInfo.filePaths;
            break;
        }
    }
    
    if (filePaths.isEmpty()) {
        AppLogger::warn(QStringLiteral("无法找到车辆文件: %1").arg(plateNumber));
        emit trajectoryLoaded(plateNumber, QList<ExcelDataReader::VehicleRecord>());
        return;
    }
    
    // Clear previous trajectory data
    m_currentTrajectory.clear();
    
    // Load data from all files and merge
    QList<ExcelDataReader::VehicleRecord> allRecords;
    int totalFiles = filePaths.size();
    int processedFiles = 0;
    
    // Reuse the existing ExcelDataReader instance to avoid creating temporary objects
    if (!m_excelReader) {
        m_excelReader = new ExcelDataReader(this);
    }
    
    for (const QString& filePath : filePaths) {
        // Disconnect any previous connections to avoid signal conflicts
        m_excelReader->disconnect();
        
        QString errorMessage;
        bool loadSuccess = false;
        
        // Connect to signals for error handling and progress tracking for this file
        // Use value capture for errorMessage to avoid lifetime issues
        connect(m_excelReader, &ExcelDataReader::errorOccurred, 
                [&errorMessage](const QString& error) {
            errorMessage = error;
        });
        
        // Connect progress signal with current file index
        int currentFileIndex = processedFiles;
        connect(m_excelReader, &ExcelDataReader::loadingProgress, 
                [this, currentFileIndex, totalFiles](int fileProgress) {
            // Calculate overall progress across all files
            int overallProgress = ((currentFileIndex * 100) + fileProgress) / totalFiles;
            emit loadingProgress(overallProgress);
        });
        
        try {
            // Load the file using the column mapping configuration
            loadSuccess = m_excelReader->loadExcelFile(filePath);
        } catch (const std::exception& e) {
            errorMessage = QString("文件读取异常: %1").arg(e.what());
            loadSuccess = false;
            AppLogger::warn(QStringLiteral("读取文件异常: %1 | %2").arg(filePath, e.what()));
        } catch (...) {
            errorMessage = "文件读取时发生未知异常";
            loadSuccess = false;
            AppLogger::warn(QStringLiteral("读取文件发生未知异常: %1").arg(filePath));
        }
        
        if (loadSuccess && errorMessage.isEmpty()) {
            QList<ExcelDataReader::VehicleRecord> fileRecords = m_excelReader->getVehicleData();
            
            // Filter records for the selected vehicle and add to collection
            for (const auto& record : fileRecords) {
                if (record.plateNumber == plateNumber) {
                    allRecords.append(record);
                }
            }
            
        } else {
            AppLogger::warn(QStringLiteral("读取文件失败: %1 | %2").arg(filePath, errorMessage));
            // Continue with other files even if one fails
        }
        
        processedFiles++;
        emit loadingProgress((processedFiles * 100) / totalFiles);
    }
    
    if (allRecords.isEmpty()) {
        AppLogger::warn(QStringLiteral("未找到车辆轨迹记录: %1").arg(plateNumber));
        emit trajectoryLoaded(plateNumber, QList<ExcelDataReader::VehicleRecord>());
        return;
    }

    finalizeLoadedTrajectory(allRecords);
}

void VehicleManager::loadVehicleTrajectoryFromDatabase(const QString& plateNumber)
{
    loadTrajectoryFromDatabase(plateNumber, {}, {});
}

void VehicleManager::loadTrajectoryFromDatabase(const QString& plateNumber,
                                                const QDate& startDate,
                                                const QDate& endDate,
                                                bool preserveAllPoints)
{
    m_currentTrajectory.clear();

    if (!m_postGisLoader || !m_postGisLoader->isConnected()) {
        AppLogger::warn(QStringLiteral("加载轨迹失败: PostGIS 未连接 | 车牌=%1").arg(plateNumber));
        emit trajectoryLoaded(plateNumber, QList<ExcelDataReader::VehicleRecord>());
        return;
    }

    emit loadingProgress(10);
    QString errorMessage;
    QList<ExcelDataReader::VehicleRecord> allRecords =
        m_postGisLoader->loadTrajectory(plateNumber, errorMessage, startDate, endDate);
    emit loadingProgress(80);

    if (allRecords.isEmpty()) {
        AppLogger::warn(QStringLiteral("加载轨迹失败: 车牌=%1 | %2").arg(plateNumber, errorMessage));
        emit trajectoryLoaded(plateNumber, QList<ExcelDataReader::VehicleRecord>());
        return;
    }

    m_selectedVehicle = plateNumber;
    finalizeLoadedTrajectory(allRecords, preserveAllPoints);
}

void VehicleManager::finalizeLoadedTrajectory(const QList<ExcelDataReader::VehicleRecord>& allRecords,
                                              bool preserveAllPoints)
{
    QList<ExcelDataReader::VehicleRecord> sortedRecords = allRecords;
    std::sort(sortedRecords.begin(), sortedRecords.end(),
              [](const ExcelDataReader::VehicleRecord& a, const ExcelDataReader::VehicleRecord& b) {
                  return a.timestamp < b.timestamp;
              });
    
    QList<ExcelDataReader::VehicleRecord> filteredRecords;
    if (preserveAllPoints) {
        filteredRecords = sortedRecords;
    } else if (!sortedRecords.isEmpty()) {
        filteredRecords.reserve(sortedRecords.size());
        filteredRecords.append(sortedRecords.first());
        
        for (int i = 1; i < sortedRecords.size(); ++i) {
            const auto& currentRecord = sortedRecords[i];
            const auto& previousRecord = filteredRecords.last();
            
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
    
    // Determine conversion direction based on coordinate system detection
    // Assume input data is in WGS84 (standard GPS coordinates)
    CoordinateConverter::CoordinateSystem sourceSystem = CoordinateConverter::WGS84;
    CoordinateConverter::CoordinateSystem targetSystem = m_coordinateConversionEnabled 
        ? CoordinateConverter::GCJ02 
        : CoordinateConverter::WGS84;
    
    // Convert each record
    for (const auto& record : m_currentTrajectory) {
        ExcelDataReader::VehicleRecord convertedRecord = record;
        
        if (m_coordinateConversionEnabled) {
            // Convert WGS84 to GCJ02
            QGeoCoordinate originalCoord(record.latitude, record.longitude);
            QGeoCoordinate convertedCoord = CoordinateConverter::wgs84ToGcj02(originalCoord);
            
            convertedRecord.latitude = convertedCoord.latitude();
            convertedRecord.longitude = convertedCoord.longitude();
        }
        // If conversion is disabled, keep original coordinates
        
        m_convertedTrajectory.append(convertedRecord);
    }
    
}

QList<ExcelDataReader::VehicleRecord> VehicleManager::getCurrentTrajectory() const
{
    return m_currentTrajectory;
}

QList<ExcelDataReader::VehicleRecord> VehicleManager::getConvertedTrajectory() const
{
    return m_convertedTrajectory;
}

QString VehicleManager::getSelectedVehicle() const
{
    return m_selectedVehicle;
}
