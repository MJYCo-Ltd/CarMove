#include "MainController.h"
#include "FolderScanner.h"
#include "VehicleManager.h"
#include "VehicleAnimationEngine.h"
#include "VehicleDataModel.h"
#include "ConfigManager.h"
#include "ErrorHandler.h"
#include <QDir>
#include <QVariantMap>
#include <QUrl>
MainController::MainController(QObject *parent)
    : QObject(parent)
    , m_coordinateConversionEnabled(false)
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
    
    connect(m_animationEngine, &VehicleAnimationEngine::vehiclePositionUpdated,
            this, &MainController::onVehiclePositionUpdate);

    m_animationEngine->setVehicleModel(m_vehicleDataModel);

    m_playbackControl = new PlaybackControl(m_animationEngine, m_vehicleDataModel, this);

    const QVariantMap persistedTarget = ConfigManager::GetInstance()->loadPersistedTargetArea();
    if (persistedTarget.contains(QStringLiteral("latitude"))
        && persistedTarget.contains(QStringLiteral("longitude"))) {
        m_targetAreaLatitude = persistedTarget.value(QStringLiteral("latitude")).toDouble();
        m_targetAreaLongitude = persistedTarget.value(QStringLiteral("longitude")).toDouble();
        m_targetAreaName = persistedTarget.value(QStringLiteral("name")).toString();
        emit targetAreaChanged();
    }
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
            if (m_playbackControl)
                m_playbackControl->stopPlayback();
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

void MainController::persistTargetAreaConfig()
{
    ConfigManager::GetInstance()->persistTargetArea(m_targetAreaLatitude, m_targetAreaLongitude,
                                                    m_targetAreaName);
}

void MainController::setTargetAreaLatitude(double lat)
{
    if (!qFuzzyCompare(1.0 + m_targetAreaLatitude, 1.0 + lat)) {
        m_targetAreaLatitude = lat;
        emit targetAreaChanged();
        persistTargetAreaConfig();
    }
}

void MainController::setTargetAreaLongitude(double lon)
{
    if (!qFuzzyCompare(1.0 + m_targetAreaLongitude, 1.0 + lon)) {
        m_targetAreaLongitude = lon;
        emit targetAreaChanged();
        persistTargetAreaConfig();
    }
}

void MainController::setTargetAreaName(const QString& name)
{
    if (m_targetAreaName != name) {
        m_targetAreaName = name;
        emit targetAreaChanged();
        persistTargetAreaConfig();
    }
}

void MainController::setTargetAreaCenter(double latitude, double longitude, const QString& name)
{
    const bool latEq = qFuzzyCompare(1.0 + m_targetAreaLatitude, 1.0 + latitude);
    const bool lonEq = qFuzzyCompare(1.0 + m_targetAreaLongitude, 1.0 + longitude);
    const bool nameEq = (m_targetAreaName == name);
    if (latEq && lonEq && nameEq)
        return;
    m_targetAreaLatitude = latitude;
    m_targetAreaLongitude = longitude;
    m_targetAreaName = name;
    emit targetAreaChanged();
    persistTargetAreaConfig();
}

// Private slots implementation → see MainControllerSlots.cpp
// Private helper methods   → see MainControllerHelpers.cpp

