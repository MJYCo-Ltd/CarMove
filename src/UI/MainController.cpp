#include "UI/MainController.h"
#include "Core/AppLogger.h"
#include "DataManagement/VehicleManager.h"
#include "Core/ConfigManager.h"
#include "Core/ErrorHandler.h"
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTime>
#include <QVariantMap>
#include <QUrl>
#include <QTimer>

namespace {
constexpr auto kTrajectoryQueryDateTimeFormat = "yyyy-MM-dd HH:mm:ss";
} // namespace

MainController::MainController(QObject *parent)
    : QObject(parent)
    , m_coordinateConversionEnabled(false)
    , m_isLoading(false)
    , m_loadingMessage("")
    , m_searchText("")
    , m_trajectoryDataManager(new TrajectoryDataManager(this))
    , m_mapServiceManager(new MapServiceManager(this))
    , m_filePathManager(new FilePathManager(this))
    , m_vehicleManager(new VehicleManager(this))
    , m_timelineManager(new TrajectoryTimelineManager(this))
{
    connect(m_timelineManager, &TrajectoryTimelineManager::timeRangeChanged,
            this, &MainController::trajectoryTimeRangeChanged);
    connect(m_timelineManager, &TrajectoryTimelineManager::currentTimeChanged,
            this, &MainController::trajectoryCurrentTimeChanged);
    connect(m_timelineManager, &TrajectoryTimelineManager::segmentsChanged,
            this, &MainController::trajectorySegmentsChanged);
    connect(m_timelineManager, &TrajectoryTimelineManager::vehiclePositionUpdated,
            this, &MainController::vehiclePositionUpdated);

    connect(m_trajectoryDataManager, &TrajectoryDataManager::scanCompleted,
            this, &MainController::onDataSourceScanCompleted);
    connect(m_trajectoryDataManager, &TrajectoryDataManager::scanError,
            this, &MainController::onDataSourceScanError);
    connect(m_trajectoryDataManager, &TrajectoryDataManager::scanProgress,
            this, &MainController::onDataSourceScanProgress);
    connect(m_trajectoryDataManager, &TrajectoryDataManager::readyChanged,
            this, &MainController::onDataSourceReadyChanged);
    connect(m_trajectoryDataManager, &TrajectoryDataManager::sourceModeChanged,
            this, &MainController::trajectorySourceModeChanged);
    connect(m_trajectoryDataManager, &TrajectoryDataManager::sourceDescriptionChanged,
            this, &MainController::currentFolderChanged);

    connect(m_vehicleManager, &VehicleManager::trajectoryLoaded,
            this, &MainController::onVehicleTrajectoryLoaded);
    connect(m_vehicleManager, &VehicleManager::trajectoryConverted,
            this, &MainController::onTrajectoryConverted);
    connect(m_vehicleManager, &VehicleManager::loadingProgress,
            this, &MainController::onVehicleLoadingProgress);

    m_vehicleManager->setDataManager(m_trajectoryDataManager);
    updateDatabaseConnectionState();

    if (m_trajectoryDataManager->useDatabaseSource()) {
        m_isLoading = true;
        m_loadingMessage = QStringLiteral("正在连接 PostGIS 数据库...");
        emit loadingChanged();
        emit loadingMessageChanged();
        m_trajectoryDataManager->refreshDatabaseSource();
    }

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
}

bool MainController::trajectorySpansMultipleDays() const
{
    return m_timelineManager && m_timelineManager->spansMultipleDays();
}

QDateTime MainController::trajectoryStartTime() const
{
    return m_timelineManager ? m_timelineManager->startTime() : QDateTime();
}

QDateTime MainController::trajectoryEndTime() const
{
    return m_timelineManager ? m_timelineManager->endTime() : QDateTime();
}

QDateTime MainController::trajectoryCurrentTime() const
{
    return m_timelineManager ? m_timelineManager->currentTime() : QDateTime();
}

void MainController::seekTrajectoryToProgress(double progress)
{
    if (m_timelineManager) {
        m_timelineManager->seekToProgress(progress);
    }
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

void MainController::reportError(const QString& message)
{
    if (!message.trimmed().isEmpty())
        emit errorOccurred(message.trimmed());
}

QVariantMap MainController::vehicleTrajectoryTimeRange(const QString& plateNumber) const
{
    const QString plate = plateNumber.trimmed();
    if (plate.isEmpty()) {
        return {};
    }

    for (const TrajectoryDataManager::VehicleInfo& info : m_vehicleInfoList) {
        if (info.plateNumber.compare(plate, Qt::CaseInsensitive) != 0) {
            continue;
        }

        QVariantMap range;
        if (info.firstSeenAt.isValid()) {
            range.insert(QStringLiteral("startTime"), info.firstSeenAt);
        }
        if (info.lastSeenAt.isValid()) {
            range.insert(QStringLiteral("endTime"), info.lastSeenAt);
        }
        return range;
    }

    return {};
}

QString MainController::currentFolder() const
{
    return m_trajectoryDataManager ? m_trajectoryDataManager->sourceDescription() : QString();
}

QString MainController::trajectorySourceMode() const
{
    return m_trajectoryDataManager ? m_trajectoryDataManager->sourceMode()
                                   : ConfigManager::GetInstance()->trajectorySourceMode();
}

bool MainController::useDatabaseTrajectorySource() const
{
    return m_trajectoryDataManager ? m_trajectoryDataManager->useDatabaseSource()
                                   : ConfigManager::GetInstance()->useDatabaseTrajectorySource();
}

bool MainController::databaseConnected() const
{
    return m_trajectoryDataManager && m_trajectoryDataManager->useDatabaseSource()
           && m_trajectoryDataManager->isReady();
}

QString MainController::databaseStatus() const
{
    return m_databaseStatus;
}

void MainController::setTrajectorySourceMode(const QString& mode)
{
    if (!m_trajectoryDataManager) {
        return;
    }

    m_isLoading = true;
    m_loadingMessage = QStringLiteral("正在切换数据源...");
    emit loadingChanged();
    emit loadingMessageChanged();

    clearVehicleDataState();
    m_trajectoryDataManager->setSourceMode(mode);
    updateDatabaseConnectionState();
    emit trajectorySourceModeChanged();
    emit currentFolderChanged();

    if (m_trajectoryDataManager->useDatabaseSource()) {
        m_loadingMessage = QStringLiteral("正在连接 PostGIS 数据库...");
        emit loadingMessageChanged();
        m_trajectoryDataManager->refreshDatabaseSource();
        return;
    }

    m_isLoading = false;
    m_loadingMessage.clear();
    emit loadingChanged();
    emit loadingMessageChanged();
}

void MainController::savePostGisSettings()
{
    ConfigManager::GetInstance()->savePostGisSettings();
}

void MainController::updateDatabaseConnectionState()
{
    if (!m_trajectoryDataManager) {
        return;
    }

    if (m_trajectoryDataManager->useDatabaseSource()) {
        m_databaseStatus = m_trajectoryDataManager->isReady()
                               ? m_trajectoryDataManager->sourceDescription()
                               : m_databaseStatus;
    } else {
        m_databaseStatus.clear();
    }
    emit databaseConnectionChanged();
}

void MainController::finishVehicleListLoad(const QList<TrajectoryDataManager::VehicleInfo>& vehicles)
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
    if (!m_trajectoryDataManager || !useDatabaseTrajectorySource()) {
        return;
    }

    m_isLoading = true;
    m_loadingMessage = QStringLiteral("正在连接 PostGIS 数据库...");
    emit loadingChanged();
    emit loadingMessageChanged();

    clearVehicleDataState();
    m_trajectoryDataManager->refreshDatabaseSource();
}

void MainController::selectFolder(const QString& folderPath)
{
    if (!m_trajectoryDataManager || useDatabaseTrajectorySource()) {
        return;
    }

    m_isLoading = true;
    m_loadingMessage = QStringLiteral("正在扫描文件夹...");
    emit loadingChanged();
    emit loadingMessageChanged();

    clearVehicleDataState();
    m_trajectoryDataManager->scanFolder(folderPath);
}

void MainController::selectVehicle(const QString& plateNumber,
                                   const QString& startDateTimeText,
                                   const QString& endDateTimeText)
{
    if (plateNumber.isEmpty()) {
        emit errorOccurred(QStringLiteral("请选择一个有效的车辆"));
        return;
    }

    if (!m_vehicleList.contains(plateNumber)) {
        emit errorOccurred(QString(QStringLiteral("车辆 %1 不在当前车辆列表中")).arg(plateNumber));
        return;
    }

    QDateTime startDateTime;
    QDateTime endDateTime;
    QString timeRangeError;
    if (!parseTrajectoryQueryTimeRange(startDateTimeText, endDateTimeText,
                                       startDateTime, endDateTime, timeRangeError)) {
        emit errorOccurred(timeRangeError);
        return;
    }

    const bool selectionChanged = (m_selectedVehicle != plateNumber);
    if (selectionChanged) {
        m_selectedVehicle = plateNumber;
        if (m_timelineManager) {
            m_timelineManager->setSelectedVehicle(plateNumber);
            m_timelineManager->invalidateSegments();
        }
        emit selectedVehicleChanged();
    }

    try {
        resetTrajectoryTimeline();
    } catch (const std::exception& e) {
        AppLogger::warn(QStringLiteral("重置时间轴时发生错误: %1").arg(e.what()));
        emit errorOccurred(QString(QStringLiteral("重置时间轴时发生错误: %1")).arg(e.what()));
    } catch (...) {
        AppLogger::warn(QStringLiteral("重置时间轴时发生未知错误"));
        emit errorOccurred(QStringLiteral("重置时间轴时发生未知错误"));
    }

    m_isLoading = true;
    if (startDateTime.isValid() && endDateTime.isValid()) {
        m_loadingMessage = QStringLiteral("正在加载车辆 %1 的轨迹 (%2 ~ %3)...")
                               .arg(plateNumber,
                                    startDateTime.toString(QString::fromLatin1(kTrajectoryQueryDateTimeFormat)),
                                    endDateTime.toString(QString::fromLatin1(kTrajectoryQueryDateTimeFormat)));
    } else {
        m_loadingMessage = QString(QStringLiteral("正在加载车辆 %1 的轨迹数据...")).arg(plateNumber);
    }
    emit loadingChanged();
    emit loadingMessageChanged();

    try {
        m_vehicleManager->selectVehicle(plateNumber, startDateTime, endDateTime);
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

bool MainController::parseTrajectoryQueryTimeRange(const QString& startDateTimeText,
                                                   const QString& endDateTimeText,
                                                   QDateTime& startDateTime,
                                                   QDateTime& endDateTime,
                                                   QString& errorMessage) const
{
    startDateTime = {};
    endDateTime = {};
    errorMessage.clear();

    const QString startText = startDateTimeText.trimmed();
    const QString endText = endDateTimeText.trimmed();
    if (startText.isEmpty() && endText.isEmpty()) {
        return true;
    }

    if (startText.isEmpty() || endText.isEmpty()) {
        errorMessage = QStringLiteral("请同时填写开始时间和结束时间，格式：yyyy-MM-dd HH:mm:ss");
        return false;
    }

    startDateTime = QDateTime::fromString(startText, QString::fromLatin1(kTrajectoryQueryDateTimeFormat));
    endDateTime = QDateTime::fromString(endText, QString::fromLatin1(kTrajectoryQueryDateTimeFormat));
    if (!startDateTime.isValid() || !endDateTime.isValid()) {
        errorMessage = QStringLiteral("时间格式无效，请使用：yyyy-MM-dd HH:mm:ss");
        return false;
    }

    if (startDateTime > endDateTime) {
        errorMessage = QStringLiteral("结束时间不能早于开始时间");
        return false;
    }

    const QDateTime now = QDateTime::currentDateTime();
    if (startDateTime > now || endDateTime > now) {
        errorMessage = QStringLiteral("开始/结束时间不能晚于当前时间");
        return false;
    }

    return true;
}

void MainController::loadTrajectoryForCapture(const QString& plateNumber,
                                              const QString& startDateIso,
                                              const QString& endDateIso)
{
    if (plateNumber.trimmed().isEmpty()) {
        emit captureTrajectoryReady(false, 0);
        return;
    }

    if (!m_trajectoryDataManager || !m_trajectoryDataManager->isReady()) {
        emit errorOccurred(QStringLiteral("轨迹数据源尚未就绪，请先选择文件夹或连接数据库"));
        emit captureTrajectoryReady(false, 0);
        return;
    }

    const QDate startDate = QDate::fromString(startDateIso, Qt::ISODate);
    const QDate endDate = QDate::fromString(endDateIso, Qt::ISODate);
    if (!startDate.isValid() || !endDate.isValid()) {
        emit captureTrajectoryReady(false, 0);
        return;
    }

    const QDateTime startDateTime(startDate, QTime(0, 0, 0));
    const QDateTime endDateTime(endDate, QTime(23, 59, 59));

    m_captureTrajectoryPending = true;
    m_captureLoadTimer.start();
    AppLogger::info(QStringLiteral("[BatchShot] trajectory.load.start plate=%1 %2~%3")
                        .arg(plateNumber.trimmed(), startDateIso, endDateIso));
    m_selectedVehicle = plateNumber.trimmed();
    emit selectedVehicleChanged();

    resetTrajectoryTimeline();

    m_isLoading = true;
    m_loadingMessage = QString(QStringLiteral("正在加载 %1 的轨迹 (%2 ~ %3)..."))
                           .arg(plateNumber, startDateIso, endDateIso);
    emit loadingChanged();
    emit loadingMessageChanged();

    m_vehicleManager->loadTrajectory(plateNumber.trimmed(), startDateTime, endDateTime, true);

    if (m_captureTrajectoryPending) {
        m_captureTrajectoryPending = false;
        m_isLoading = false;
        m_loadingMessage.clear();
        emit loadingChanged();
        emit loadingMessageChanged();
        QTimer::singleShot(0, this, [this]() {
            emit captureTrajectoryReady(false, 0);
        });
    }
}

QString MainController::normalizeLocalPath(const QString& path) const
{
    return m_filePathManager ? m_filePathManager->normalizeLocalPath(path) : path;
}

bool MainController::ensureScreenshotOutputDirectory(const QString& folderPath) const
{
    return m_filePathManager && m_filePathManager->ensureScreenshotOutputDirectory(folderPath);
}

QString MainController::screenshotFilePath(const QString& folderPath,
                                           const QString& plateNumber,
                                           const QString& startDateIso,
                                           const QString& endDateIso) const
{
    return m_filePathManager
               ? m_filePathManager->screenshotFilePath(folderPath, plateNumber, startDateIso, endDateIso)
               : QString();
}

QString MainController::targetAreaScreenshotFilePath(const QString& folderPath,
                                                     const QString& plateNumber,
                                                     const QString& startDateIso,
                                                     const QString& endDateIso) const
{
    return m_filePathManager ? m_filePathManager->targetAreaScreenshotFilePath(folderPath, plateNumber,
                                                                               startDateIso, endDateIso)
                             : QString();
}

bool MainController::screenshotFileExists(const QString& folderPath,
                                          const QString& plateNumber,
                                          const QString& startDateIso,
                                          const QString& endDateIso) const
{
    return m_filePathManager
           && m_filePathManager->screenshotFileExists(folderPath, plateNumber, startDateIso, endDateIso);
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

void MainController::prepareForApplicationShutdown()
{
    if (m_trajectoryDataManager) {
        m_trajectoryDataManager->releaseDatabaseConnection();
    }
}

// Private slots implementation → see MainControllerSlots.cpp
// Private helper methods   → see MainControllerHelpers.cpp

