#include "MainController.h"
#include "FolderScanner.h"
#include "VehicleManager.h"
#include "VehicleAnimationEngine.h"
#include "VehicleDataModel.h"
#include "ConfigManager.h"
#include "ErrorHandler.h"
#include "PostGisDatabaseConfig.h"
#include "ParseData/ExcelFilePath.h"
#include <QDate>
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
    , m_postGisLoader(new PostGisTrajectoryLoader(this))
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

    m_vehicleManager->setPostGisLoader(m_postGisLoader);
    applyTrajectorySourceMode();

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

QString MainController::trajectorySourceMode() const
{
    return ConfigManager::GetInstance()->trajectorySourceMode();
}

bool MainController::useDatabaseTrajectorySource() const
{
    return ConfigManager::GetInstance()->useDatabaseTrajectorySource();
}

void MainController::setTrajectorySourceMode(const QString& mode)
{
    ConfigManager::GetInstance()->setTrajectorySourceMode(mode);
    applyTrajectorySourceMode();
    emit trajectorySourceModeChanged();
}

void MainController::savePostGisSettings()
{
    ConfigManager::GetInstance()->savePostGisSettings();
}

void MainController::applyTrajectorySourceMode()
{
    const bool databaseMode = useDatabaseTrajectorySource();
    m_vehicleManager->setDatabaseMode(databaseMode);

    if (databaseMode) {
        m_currentFolder.clear();
        emit currentFolderChanged();
        connectPostGisDatabase();
    } else {
        if (m_postGisLoader) {
            m_postGisLoader->disconnectDatabase();
        }
        m_databaseConnected = false;
        m_databaseStatus.clear();
        emit databaseConnectionChanged();
        clearVehicleDataState();
    }
}

void MainController::finishVehicleListLoad(const QList<FolderScanner::VehicleInfo>& vehicles)
{
    m_vehicleInfoList = vehicles;
    m_vehicleList.clear();

    for (const auto& vehicle : vehicles) {
        m_vehicleList.append(vehicle.plateNumber);
    }

    m_vehicleManager->setVehicleList(vehicles);
    updateFilteredVehicleList();

    m_isLoading = false;
    m_loadingMessage.clear();
    emit loadingChanged();
    emit loadingMessageChanged();
    emit vehicleListChanged();
}

void MainController::clearVehicleDataState()
{
    m_vehicleList.clear();
    m_selectedVehicle.clear();
    m_vehicleInfoList.clear();
    m_vehicleManager->setVehicleList({});
    emit vehicleListChanged();
    emit selectedVehicleChanged();
    updateFilteredVehicleList();
}

void MainController::connectPostGisDatabase()
{
    if (!useDatabaseTrajectorySource()) {
        return;
    }

    m_isLoading = true;
    m_loadingMessage = QStringLiteral("正在连接 PostGIS 数据库...");
    emit loadingChanged();
    emit loadingMessageChanged();

    clearVehicleDataState();

    QString errorMessage;
    const PostGisDatabaseConfig config = ConfigManager::GetInstance()->postGisDatabaseConfig();
    if (!m_postGisLoader->connectDatabase(config, errorMessage)) {
        m_databaseConnected = false;
        m_databaseStatus = errorMessage;
        m_isLoading = false;
        emit databaseConnectionChanged();
        emit loadingChanged();
        emit loadingMessageChanged();
        emit errorOccurred(errorMessage);
        return;
    }

    m_databaseConnected = true;
    m_databaseStatus = config.connectionSummary();
    emit databaseConnectionChanged();

    const QList<FolderScanner::VehicleInfo> vehicles = m_postGisLoader->listVehicles(errorMessage);
    if (vehicles.isEmpty()) {
        m_postGisLoader->disconnectDatabase();
        m_databaseConnected = false;
        m_databaseStatus.clear();
        m_isLoading = false;
        emit databaseConnectionChanged();
        emit loadingChanged();
        emit loadingMessageChanged();
        emit errorOccurred(errorMessage);
        return;
    }

    finishVehicleListLoad(vehicles);
    m_currentFolder = config.connectionSummary();
    emit currentFolderChanged();
}

void MainController::selectFolder(const QString& folderPath)
{
    if (useDatabaseTrajectorySource()) {
        return;
    }
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
    if (plateNumber.isEmpty()) {
        emit errorOccurred(QStringLiteral("请选择一个有效的车辆"));
        return;
    }

    if (!m_vehicleList.contains(plateNumber)) {
        emit errorOccurred(QString(QStringLiteral("车辆 %1 不在当前车辆列表中")).arg(plateNumber));
        return;
    }

    if (m_selectedVehicle != plateNumber) {
        m_selectedVehicle = plateNumber;
        emit selectedVehicleChanged();

        try {
            if (m_playbackControl) {
                m_playbackControl->stopPlayback();
            }
        } catch (const std::exception& e) {
            qWarning() << "Error stopping playback:" << e.what();
            emit errorOccurred(QString(QStringLiteral("停止播放时发生错误: %1")).arg(e.what()));
        } catch (...) {
            qWarning() << "Unknown error stopping playback";
            emit errorOccurred(QStringLiteral("停止播放时发生未知错误"));
        }

        m_isLoading = true;
        m_loadingMessage = QString(QStringLiteral("正在加载车辆 %1 的轨迹数据...")).arg(plateNumber);
        emit loadingChanged();
        emit loadingMessageChanged();

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

void MainController::loadTrajectoryForCapture(const QString& plateNumber,
                                              const QString& startDateIso,
                                              const QString& endDateIso)
{
    if (plateNumber.trimmed().isEmpty()) {
        emit captureTrajectoryReady(false, 0);
        return;
    }

    if (!useDatabaseTrajectorySource() || !m_databaseConnected) {
        emit errorOccurred(QStringLiteral("PostGIS 数据库未连接，请检查 CarMoveTracker.ini 配置"));
        emit captureTrajectoryReady(false, 0);
        return;
    }

    const QDate startDate = QDate::fromString(startDateIso, Qt::ISODate);
    const QDate endDate = QDate::fromString(endDateIso, Qt::ISODate);
    if (!startDate.isValid() || !endDate.isValid()) {
        emit captureTrajectoryReady(false, 0);
        return;
    }

    m_captureTrajectoryPending = true;
    m_selectedVehicle = plateNumber.trimmed();
    emit selectedVehicleChanged();

    if (m_playbackControl) {
        m_playbackControl->stopPlayback();
    }

    m_isLoading = true;
    m_loadingMessage = QString(QStringLiteral("正在加载 %1 的轨迹 (%2 ~ %3)..."))
                           .arg(plateNumber, startDateIso, endDateIso);
    emit loadingChanged();
    emit loadingMessageChanged();

    m_vehicleManager->loadTrajectoryFromDatabase(plateNumber.trimmed(), startDate, endDate, true);
}

QString MainController::normalizeLocalPath(const QString& path) const
{
    return ExcelFilePath::normalizeLocalFilePath(path);
}

bool MainController::ensureScreenshotOutputDirectory(const QString& folderPath) const
{
    const QString localPath = ExcelFilePath::normalizeLocalFilePath(folderPath);
    if (localPath.trimmed().isEmpty()) {
        return false;
    }
    return QDir().mkpath(localPath);
}

QString MainController::screenshotFilePath(const QString& folderPath,
                                           const QString& plateNumber,
                                           const QString& startDateIso,
                                           const QString& endDateIso) const
{
    const QString localFolder = ExcelFilePath::normalizeLocalFilePath(folderPath);
    const QString safePlate = ExcelFilePath::sanitizePlateForFilename(plateNumber);

    const QString fileName =
        safePlate + QLatin1Char('_') + startDateIso + QLatin1Char('_') + endDateIso + QStringLiteral(".png");
    return QDir(localFolder).filePath(fileName);
}

void MainController::toggleCoordinateConversion()
{
    try {
        setCoordinateConversionEnabled(!m_coordinateConversionEnabled);

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

