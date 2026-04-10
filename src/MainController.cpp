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

// Private slots implementation → see MainControllerSlots.cpp
// Private helper methods   → see MainControllerHelpers.cpp

