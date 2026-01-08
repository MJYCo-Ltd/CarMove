#include "VehicleManager.h"
#include "CoordinateConverter.h"

VehicleManager::VehicleManager(QObject *parent)
    : QObject(parent)
    , m_coordinateConversionEnabled(false)
    , m_excelReader(new ExcelDataReader(this))
{
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
    if (m_selectedVehicle != plateNumber) {
        m_selectedVehicle = plateNumber;
        
        // Clear previous trajectory data
        m_currentTrajectory.clear();
        m_convertedTrajectory.clear();
        
        emit vehicleSelected(plateNumber);
        
        // Find file paths for this vehicle
        QStringList filePaths;
        for (const auto& vehicleInfo : m_vehicleList) {
            if (vehicleInfo.plateNumber == plateNumber) {
                filePaths = vehicleInfo.filePaths;
                break;
            }
        }
        
        // Check if column mapping is required for these files
        // Note: Column mapping is now handled automatically in MainController
        // No need to emit signal or wait for user input
        
        // Load trajectory data for the selected vehicle
        loadVehicleTrajectory(plateNumber);
    }
}

void VehicleManager::loadVehicleTrajectory(const QString& plateNumber)
{
    if (plateNumber.isEmpty()) {
        qWarning() << "Cannot load trajectory: plate number is empty";
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
        qWarning() << "Cannot find file paths for vehicle:" << plateNumber;
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
            qWarning() << "Exception loading file" << filePath << ":" << e.what();
        } catch (...) {
            errorMessage = "文件读取时发生未知异常";
            loadSuccess = false;
            qWarning() << "Unknown exception loading file" << filePath;
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
            qWarning() << "Failed to load file" << filePath << ":" << errorMessage;
            // Continue with other files even if one fails
        }
        
        processedFiles++;
        emit loadingProgress((processedFiles * 100) / totalFiles);
    }
    
    if (allRecords.isEmpty()) {
        qWarning() << "No records found for vehicle:" << plateNumber;
        emit trajectoryLoaded(plateNumber, QList<ExcelDataReader::VehicleRecord>());
        return;
    }
    
    // Sort all records by timestamp to create a chronological trajectory
    std::sort(allRecords.begin(), allRecords.end(), 
              [](const ExcelDataReader::VehicleRecord& a, const ExcelDataReader::VehicleRecord& b) {
                  return a.timestamp < b.timestamp;
              });
    
    // Filter out stationary vehicle data points (speed = 0 and same mileage as previous record)
    QList<ExcelDataReader::VehicleRecord> filteredRecords;
    if (!allRecords.isEmpty()) {
        filteredRecords.reserve(allRecords.size()); // Reserve space for better performance
        
        // Always include the first record
        filteredRecords.append(allRecords.first());
        
        int originalCount = allRecords.size();
        int filteredCount = 0;
        
        for (int i = 1; i < allRecords.size(); ++i) {
            const auto& currentRecord = allRecords[i];
            const auto& previousRecord = filteredRecords.last(); // Use last filtered record for comparison
            
            // Check if vehicle is stationary (speed = 0 and same mileage)
            bool isStationary = (currentRecord.speed == 0.0) && 
                               (currentRecord.totalMileage == previousRecord.totalMileage) &&
                               (!currentRecord.totalMileage.isEmpty()); // Only filter if mileage data exists
            
            if (!isStationary) {
                // Include this record if vehicle is moving or mileage changed
                filteredRecords.append(currentRecord);
            } else {
                filteredCount++;
            }
        }
    }
    
    m_currentTrajectory = filteredRecords;
    
    // Apply coordinate conversion if enabled
    if (m_coordinateConversionEnabled) {
        applyCoordinateConversionToCurrentTrajectory();
    } else {
        m_convertedTrajectory = m_currentTrajectory;
    }
    
    emit trajectoryLoaded(m_selectedVehicle, m_convertedTrajectory);
    emit loadingProgress(100); // Complete
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

bool VehicleManager::isCoordinateConversionEnabled() const
{
    return m_coordinateConversionEnabled;
}

QStringList VehicleManager::getAvailableVehicles() const
{
    QStringList vehicles;
    for (const auto& vehicleInfo : m_vehicleList) {
        vehicles.append(vehicleInfo.plateNumber);
    }
    return vehicles;
}

bool VehicleManager::hasTrajectoryData() const
{
    return !m_convertedTrajectory.isEmpty();
}
