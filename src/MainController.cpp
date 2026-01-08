#include "MainController.h"
#include "FolderScanner.h"
#include "VehicleManager.h"
#include "VehicleAnimationEngine.h"
#include "VehicleDataModel.h"
#include "ErrorHandler.h"
#include <QDir>
#include <QVariantMap>
#include <QStandardPaths>
#include <QUrl>

MainController::MainController(QObject *parent)
    : QObject(parent)
    , m_coordinateConversionEnabled(false)
    , m_isPlaying(false)
    , m_playbackProgress(0.0)
    , m_isLoading(false)
    , m_loadingMessage("")
    , m_searchText("")
    , m_folderScanner(new FolderScanner(this))
    , m_vehicleManager(new VehicleManager(this))
    , m_animationEngine(new VehicleAnimationEngine(this))
    , m_vehicleDataModel(new VehicleDataModel(this))
{
    // Connect FolderScanner signals
    connect(m_folderScanner, &FolderScanner::scanCompleted,
            this, &MainController::onFolderScanCompleted);
    connect(m_folderScanner, &FolderScanner::scanError,
            this, &MainController::onFolderScanError);
    connect(m_folderScanner, &FolderScanner::scanProgress,
            this, &MainController::onFolderScanProgress);
    
    // Connect VehicleManager signals
    connect(m_vehicleManager, &VehicleManager::trajectoryLoaded,
            this, &MainController::onVehicleTrajectoryLoaded);
    connect(m_vehicleManager, &VehicleManager::trajectoryConverted,
            this, &MainController::onTrajectoryConverted);
    connect(m_vehicleManager, &VehicleManager::loadingProgress,
            this, &MainController::onVehicleLoadingProgress);
    
    // Connect VehicleAnimationEngine signals
    connect(m_animationEngine, &VehicleAnimationEngine::currentTimeChanged,
            this, &MainController::onAnimationCurrentTimeChanged);
    connect(m_animationEngine, &VehicleAnimationEngine::progressChanged,
            this, &MainController::onAnimationProgressChanged);
    connect(m_animationEngine, QOverload<VehicleAnimationEngine::PlaybackState>::of(&VehicleAnimationEngine::playbackStateChanged),
            this, &MainController::onAnimationPlaybackStateChanged);
    connect(m_animationEngine, &VehicleAnimationEngine::vehiclePositionUpdated,
            this, &MainController::onVehiclePositionUpdate);
    
    // Set up animation engine with data model
    m_animationEngine->setVehicleModel(m_vehicleDataModel);
    
}

MainController::~MainController()
{
    // Components will be deleted automatically by Qt parent-child system
}

void MainController::setCoordinateConversionEnabled(bool enabled)
{
    if (m_coordinateConversionEnabled != enabled) {
        m_coordinateConversionEnabled = enabled;
        emit coordinateConversionChanged();
        
        // Apply conversion to current trajectory if vehicle is selected
        if (!m_selectedVehicle.isEmpty()) {
            m_vehicleManager->applyCoordinateConversion(enabled);
        }
    }
}

void MainController::setSearchText(const QString& text)
{
    if (m_searchText != text) {
        m_searchText = text;
        emit searchTextChanged();
        updateFilteredVehicleList();
    }
}


void MainController::clearSearch()
{
    setSearchText("");
}

void MainController::updateFilteredVehicleList()
{
    if (m_searchText.isEmpty()) {
        // No search text, show all vehicles
        m_filteredVehicleList = m_vehicleList;
    } else {
        // Simple prefix matching - exactly what user wants
        m_filteredVehicleList.clear();
        QString searchText = m_searchText; // Keep original case for exact matching
        
        for (const QString& vehicle : m_vehicleList) {
            if (vehicle.startsWith(searchText, Qt::CaseInsensitive)) {
                m_filteredVehicleList.append(vehicle);
            }
        }
    }
    
    emit filteredVehicleListChanged();
}

void MainController::selectFolder(const QString& folderPath)
{
    // Validate folder path
    if (folderPath.isEmpty()) {
        emit errorOccurred("请选择一个有效的文件夹路径");
        return;
    }
    
    // Normalize path
    QString normalizedPath = QUrl(folderPath).toLocalFile();
    
    if (m_currentFolder != normalizedPath) {
        m_currentFolder = normalizedPath;
        emit currentFolderChanged();
        
        // Clear current state
        m_vehicleList.clear();
        m_selectedVehicle.clear();
        m_vehicleInfoList.clear();
        emit vehicleListChanged();
        emit selectedVehicleChanged();
        
        // Set loading state
        m_isLoading = true;
        m_loadingMessage = "正在扫描文件夹...";
        emit loadingChanged();
        emit loadingMessageChanged();
        
        // Additional validation before scanning
        QDir dir(normalizedPath);
        if (!dir.exists()) {
            m_isLoading = false;
            emit loadingChanged();
            emit errorOccurred(HANDLE_FILE_ERROR(normalizedPath, "访问文件夹"));
            return;
        }
        
        // Check if folder is readable
        QFileInfo dirInfo(normalizedPath);
        if (!dirInfo.isReadable()) {
            m_isLoading = false;
            emit loadingChanged();
            emit errorOccurred(HANDLE_FILE_ERROR(normalizedPath, "读取文件夹"));
            return;
        }
        
        // Start folder scanning
        try {
            m_folderScanner->scanFolder(normalizedPath);
        } catch (const std::exception& e) {
            m_isLoading = false;
            emit loadingChanged();
            emit errorOccurred(HANDLE_SYSTEM_ERROR("扫描文件夹", e.what()));
        } catch (...) {
            m_isLoading = false;
            emit loadingChanged();
            emit errorOccurred(HANDLE_SYSTEM_ERROR("扫描文件夹", "未知异常"));
        }
    }
}

void MainController::selectVehicle(const QString& plateNumber)
{
    // Validate plate number
    if (plateNumber.isEmpty()) {
        emit errorOccurred("请选择一个有效的车辆");
        return;
    }
    
    // Check if vehicle exists in the list
    if (!m_vehicleList.contains(plateNumber)) {
        emit errorOccurred(QString("车辆 %1 不在当前车辆列表中").arg(plateNumber));
        return;
    }
    
    if (m_selectedVehicle != plateNumber) {
        m_selectedVehicle = plateNumber;
        emit selectedVehicleChanged();
        
        // Stop any current playback
        try {
            stopPlayback();
        } catch (const std::exception& e) {
            qWarning() << "Error stopping playback:" << e.what();
            emit errorOccurred(QString("停止播放时发生错误: %1").arg(e.what()));
        } catch (...) {
            qWarning() << "Unknown error stopping playback";
            emit errorOccurred("停止播放时发生未知错误");
        }
        
        // Set loading state
        m_isLoading = true;
        m_loadingMessage = QString("正在加载车辆 %1 的轨迹数据...").arg(plateNumber);
        emit loadingChanged();
        emit loadingMessageChanged();
        
        // Load vehicle trajectory with error handling
        try {
            m_vehicleManager->selectVehicle(plateNumber);
            m_vehicleManager->loadVehicleTrajectory(plateNumber);
        } catch (const std::exception& e) {
            m_isLoading = false;
            emit loadingChanged();
            emit errorOccurred(HANDLE_SYSTEM_ERROR("加载车辆轨迹", e.what()));
        } catch (...) {
            m_isLoading = false;
            emit loadingChanged();
            emit errorOccurred(HANDLE_SYSTEM_ERROR("加载车辆轨迹", "未知异常"));
        }
    }
}

void MainController::toggleCoordinateConversion()
{
    try {
        setCoordinateConversionEnabled(!m_coordinateConversionEnabled);
        
        // Trigger trajectory conversion signal
        if (!m_selectedVehicle.isEmpty()) {
            emit trajectoryConverted();
        }
    } catch (const std::exception& e) {
        emit errorOccurred(HANDLE_COORD_ERROR(QString("坐标转换切换失败: %1").arg(e.what())));
    } catch (...) {
        emit errorOccurred(HANDLE_COORD_ERROR("坐标转换切换时发生未知错误"));
    }
}

QVariantList MainController::getConvertedTrajectory()
{
    QVariantList result;
    
    try {
        if (m_vehicleManager) {
            auto trajectory = m_coordinateConversionEnabled ? 
                             m_vehicleManager->getConvertedTrajectory() : 
                             m_vehicleManager->getCurrentTrajectory();
            
            for (const auto& record : trajectory) {
                result.append(vehicleRecordToVariant(record));
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "Error getting converted trajectory:" << e.what();
        emit errorOccurred(HANDLE_COORD_ERROR(QString("获取转换后轨迹失败: %1").arg(e.what())));
    } catch (...) {
        qWarning() << "Unknown error getting converted trajectory";
        emit errorOccurred(HANDLE_COORD_ERROR("获取转换后轨迹时发生未知错误"));
    }
    
    return result;
}

QVariantList MainController::getCurrentTrajectory()
{
    QVariantList result;
    
    if (m_vehicleManager) {
        auto trajectory = m_vehicleManager->getCurrentTrajectory();
        
        for (const auto& record : trajectory) {
            result.append(vehicleRecordToVariant(record));
        }
    }
    
    return result;
}

void MainController::startPlayback()
{
    if (m_animationEngine && !m_selectedVehicle.isEmpty()) {
        m_animationEngine->play();
    }
}

void MainController::pausePlayback()
{
    if (m_animationEngine) {
        m_animationEngine->pause();
    }
}

void MainController::stopPlayback()
{
    if (m_animationEngine) {
        m_animationEngine->stop();
    }
}

void MainController::setPlaybackSpeed(double speed)
{
    if (m_animationEngine) {
        m_animationEngine->setPlaybackSpeed(speed);
    }
}

void MainController::seekToTime(const QDateTime& time)
{
    if (m_animationEngine) {
        m_animationEngine->seekToTime(time);
    }
}

void MainController::seekToProgress(double progress)
{
    if (m_animationEngine) {
        m_animationEngine->seekToProgress(progress);
    }
}

QString MainController::getVehicleInfo(const QString& plateNumber)
{
    for (const auto& info : m_vehicleInfoList) {
        if (info.plateNumber == plateNumber) {
            if (info.firstTimestamp.isValid() && info.lastTimestamp.isValid()) {
                // 如果有时间信息（已加载过数据）
                return QString("Files: %1, Records: %2, Time: %3 - %4")
                       .arg(info.filePaths.size())
                       .arg(info.recordCount)
                       .arg(info.firstTimestamp.toString("yyyy-MM-dd hh:mm"))
                       .arg(info.lastTimestamp.toString("yyyy-MM-dd hh:mm"));
            } else {
                // 只有文件信息（未加载数据）
                return QString("Files: %1 (click to load data)")
                       .arg(info.filePaths.size());
            }
        }
    }
    return QString("No information available");
}

void MainController::refreshVehicleList()
{
    if (!m_currentFolder.isEmpty()) {
        m_folderScanner->scanFolder(m_currentFolder);
    }
}

QDateTime MainController::progressToTime(double progress)
{
    if (!m_startTime.isValid() || !m_endTime.isValid()) {
        return QDateTime();
    }
    
    progress = qBound(0.0, progress, 1.0);
    qint64 totalMs = m_startTime.msecsTo(m_endTime);
    qint64 targetMs = static_cast<qint64>(totalMs * progress);
    return m_startTime.addMSecs(targetMs);
}

double MainController::timeToProgress(const QDateTime& time)
{
    if (!m_startTime.isValid() || !m_endTime.isValid() || !time.isValid()) {
        return 0.0;
    }
    
    qint64 totalMs = m_startTime.msecsTo(m_endTime);
    if (totalMs <= 0) {
        return 0.0;
    }
    
    qint64 currentMs = m_startTime.msecsTo(time);
    return qBound(0.0, static_cast<double>(currentMs) / totalMs, 1.0);
}

void MainController::setDraggingMode(bool isDragging)
{
    if (m_animationEngine) {
        m_animationEngine->setDraggingMode(isDragging);
    }
}

// Private slots implementation

void MainController::onFolderScanCompleted(const QList<FolderScanner::VehicleInfo>& vehicles)
{
    m_vehicleInfoList = vehicles;
    m_vehicleList.clear();
    
    for (const auto& vehicle : vehicles) {
        m_vehicleList.append(vehicle.plateNumber);
    }
    
    // Pass vehicle list to VehicleManager
    m_vehicleManager->setVehicleList(vehicles);
    
    // Update filtered list
    updateFilteredVehicleList();
    
    // Clear loading state
    m_isLoading = false;
    m_loadingMessage = "";
    emit loadingChanged();
    emit loadingMessageChanged();
    
    emit vehicleListChanged();
    emit folderScanned(true, QString("成功找到 %1 辆车的数据").arg(vehicles.size()));
    
}

void MainController::onFolderScanError(const QString& error)
{
    // Clear loading state
    m_isLoading = false;
    m_loadingMessage = "";
    emit loadingChanged();
    emit loadingMessageChanged();
    
    emit folderScanned(false, error);
    emit errorOccurred(QString("文件夹扫描错误: %1").arg(error));
}

void MainController::onFolderScanProgress(int percentage)
{
    m_loadingMessage = QString("正在扫描文件夹... %1%").arg(percentage);
    emit loadingMessageChanged();
    emit loadingProgress(percentage);
}

void MainController::onVehicleTrajectoryLoaded(const QString& plateNumber, 
                                              const QList<ExcelDataReader::VehicleRecord>& trajectory)
{
    if (plateNumber == m_selectedVehicle) {
        // First, set up the vehicle data model with the trajectory data
        setupVehicleDataModel();
        
        // Update time range from the data model - this is crucial for correct progress bar display
        updateTimeRange();
        
        // Log information about the loaded data for long-term analysis
        if (!trajectory.isEmpty()) {
            QDateTime firstTime = trajectory.first().timestamp;
            QDateTime lastTime = trajectory.last().timestamp;
            qint64 totalDays = firstTime.daysTo(lastTime);
            qint64 totalHours = firstTime.secsTo(lastTime) / 3600;
            
            // Provide user feedback about data span
            QString spanInfo;
            if (totalDays > 365) {
                spanInfo = QString("跨度 %1 年").arg(totalDays / 365);
            } else if (totalDays > 30) {
                spanInfo = QString("跨度 %1 个月").arg(totalDays / 30);
            } else if (totalDays > 7) {
                spanInfo = QString("跨度 %1 周").arg(totalDays / 7);
            } else if (totalDays > 0) {
                spanInfo = QString("跨度 %1 天").arg(totalDays);
            } else {
                spanInfo = QString("跨度 %1 小时").arg(totalHours);
            }
            
            emit trajectoryLoaded(true, QString("成功加载 %1 个轨迹点，%2").arg(trajectory.size()).arg(spanInfo));
        } else {
            emit trajectoryLoaded(true, "成功加载轨迹数据");
        }
        
        // Clear loading state
        m_isLoading = false;
        m_loadingMessage = "";
        emit loadingChanged();
        emit loadingMessageChanged();
        
        // Reset animation to start position and ensure animation engine has the correct time range
        if (m_animationEngine && !trajectory.isEmpty()) {
            // Make sure animation engine has the updated time range
            m_animationEngine->setVehicleModel(m_vehicleDataModel);
            m_animationEngine->stop();
            m_animationEngine->seekToProgress(0.0);
            
            // Set current time to start time for proper initialization
            m_currentTime = m_startTime;
            emit currentTimeChanged();
        }
    }
}

void MainController::onTrajectoryConverted(const QString& plateNumber,
                                          const QList<ExcelDataReader::VehicleRecord>& convertedTrajectory)
{
    if (plateNumber == m_selectedVehicle) {
        // Update the data model with converted trajectory
        setupVehicleDataModel();
        
        // Update time range after conversion
        updateTimeRange();
        
        emit trajectoryConverted();
        
        // Update vehicle positions after conversion
        if (m_animationEngine) {
            m_animationEngine->updateVehiclePositions();
        }
    }
}

void MainController::onVehicleLoadingProgress(int percentage)
{
    m_loadingMessage = QString("正在加载轨迹数据... %1%").arg(percentage);
    emit loadingMessageChanged();
    emit loadingProgress(percentage);
}

void MainController::onAnimationCurrentTimeChanged(const QDateTime& time)
{
    if (m_currentTime != time) {
        m_currentTime = time;
        emit currentTimeChanged();
    }
}

void MainController::onAnimationProgressChanged(double progress)
{
    if (qAbs(m_playbackProgress - progress) > 0.001) {
        m_playbackProgress = progress;
        emit progressChanged();
    }
}

void MainController::onAnimationPlaybackStateChanged(VehicleAnimationEngine::PlaybackState state)
{
    bool wasPlaying = m_isPlaying;
    m_isPlaying = (state == VehicleAnimationEngine::Playing);
    
    if (wasPlaying != m_isPlaying) {
        emit playbackStateChanged();
    }
}

void MainController::onVehiclePositionUpdate(const QString& plateNumber, 
                                            const QGeoCoordinate& position, 
                                            int direction, double speed)
{
    emit vehiclePositionUpdated(plateNumber, position, direction, speed);
}


// Private helper methods

void MainController::updateTimeRange()
{
    if (m_vehicleDataModel) {
        QDateTime newStartTime = m_vehicleDataModel->getStartTime();
        QDateTime newEndTime = m_vehicleDataModel->getEndTime();
        
        bool changed = false;
        if (m_startTime != newStartTime) {
            m_startTime = newStartTime;
            changed = true;
        }
        if (m_endTime != newEndTime) {
            m_endTime = newEndTime;
            changed = true;
        }
        
        if (changed) {
            emit timeRangeChanged();
            
            // Also update current time to start time if not set
            if (!m_currentTime.isValid() || m_currentTime < m_startTime || m_currentTime > m_endTime) {
                m_currentTime = m_startTime;
                emit currentTimeChanged();
            }
        }
    } else {
    }
}

void MainController::setupVehicleDataModel()
{
    if (m_vehicleManager && m_vehicleDataModel) {
        auto trajectory = m_coordinateConversionEnabled ? 
                         m_vehicleManager->getConvertedTrajectory() : 
                         m_vehicleManager->getCurrentTrajectory();
        
        // Set the trajectory data in the model
        m_vehicleDataModel->setVehicleData(trajectory);
        
        // Ensure the animation engine has the updated model
        if (m_animationEngine) {
            m_animationEngine->setVehicleModel(m_vehicleDataModel);
        }
        
    }
}

QVariantMap MainController::vehicleRecordToVariant(const ExcelDataReader::VehicleRecord& record)
{
    QVariantMap result;
    result["plateNumber"] = record.plateNumber;
    result["vehicleColor"] = record.vehicleColor;
    result["speed"] = record.speed;
    result["longitude"] = record.longitude;
    result["latitude"] = record.latitude;
    result["direction"] = record.direction;
    result["distance"] = record.distance;
    result["timestamp"] = record.timestamp;
    result["totalMileage"] = record.totalMileage;
    result["coordinate"] = QVariant::fromValue(QGeoCoordinate(record.latitude, record.longitude));
    return result;
}

int MainController::calculateVisitDays(const QString& plateNumber, double targetLat, double targetLon, double radiusMeters)
{
    if (!m_vehicleManager) {
        return 0;
    }
    
    // 如果请求的车辆不是当前选中的车辆，返回0
    if (m_vehicleManager->getSelectedVehicle() != plateNumber) {
        return 0;
    }
    
    // 获取当前车辆的轨迹数据
    auto trajectory = m_vehicleManager->getCurrentTrajectory();
    if (trajectory.isEmpty()) {
        return 0;
    }
    
    // 目标坐标
    QGeoCoordinate targetCoord(targetLat, targetLon);
    
    // 用于存储到达目标区域的日期
    QSet<QDate> visitDates;
    
    // 遍历轨迹点，检查是否在目标区域内
    for (const auto& record : trajectory) {
        QGeoCoordinate currentCoord(record.latitude, record.longitude);
        
        // 计算距离（米）
        double distance = targetCoord.distanceTo(currentCoord);
        
        // 如果在指定半径内，记录日期
        if (distance <= radiusMeters) {
            QDate visitDate = record.timestamp.date();
            visitDates.insert(visitDate);
        }
    }
    
    return visitDates.size();
}

QString MainController::getDocumentsPath()
{
    // 获取文档路径并创建截图目录
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString screenshotDir = documentsPath + "/CarMove_Screenshots";
    
    QDir dir;
    if (!dir.exists(screenshotDir)) {
        if (!dir.mkpath(screenshotDir)) {
            qWarning() << "无法创建截图目录:" << screenshotDir;
            return documentsPath; // 返回文档路径作为备选
        }
    }
    
    return documentsPath;
}
