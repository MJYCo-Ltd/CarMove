#include "DataManagement/TrajectoryDataManager.h"

#include "Core/ConfigManager.h"
#include "Core/ErrorHandler.h"
#include "DataManagement/PostGisDataManager.h"
#include "ExcelDriver/ExcelTrajectoryManager.h"
#include "DataManagement/PostGisDatabaseConfig.h"
#include "Core/AppLogger.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace {

QString normalizeFolderPath(const QString& folderPath)
{
    if (folderPath.isEmpty()) {
        return QString();
    }
    return QUrl(folderPath).toLocalFile();
}

} // namespace

TrajectoryDataManager::TrajectoryDataManager(QObject* parent)
    : QObject(parent)
    , m_postGisData(new PostGisDataManager(this))
    , m_excelTrajectory(new ExcelTrajectoryManager(this))
    , m_sourceMode(ConfigManager::GetInstance()->trajectorySourceMode())
{
    connect(m_excelTrajectory, &ExcelTrajectoryManager::scanCompleted,
            this, &TrajectoryDataManager::onExcelScanCompleted);
    connect(m_excelTrajectory, &ExcelTrajectoryManager::scanError,
            this, &TrajectoryDataManager::onExcelScanError);
    connect(m_excelTrajectory, &ExcelTrajectoryManager::scanProgress,
            this, &TrajectoryDataManager::onExcelScanProgress);
    connect(m_excelTrajectory, &ExcelTrajectoryManager::loadProgress,
            this, &TrajectoryDataManager::onExcelLoadProgress);
}

TrajectoryDataManager::~TrajectoryDataManager() = default;

QString TrajectoryDataManager::sourceMode() const
{
    return m_sourceMode;
}

bool TrajectoryDataManager::useDatabaseSource() const
{
    return m_sourceMode == QStringLiteral("database");
}

bool TrajectoryDataManager::isReady() const
{
    return m_ready;
}

QString TrajectoryDataManager::sourceDescription() const
{
    return m_sourceDescription;
}

bool TrajectoryDataManager::supportsDateRangeQuery() const
{
    return useDatabaseSource();
}

void TrajectoryDataManager::setSourceMode(const QString& mode)
{
    const QString normalized =
        mode == QStringLiteral("database") ? QStringLiteral("database") : QStringLiteral("folder");
    if (m_sourceMode == normalized) {
        return;
    }

    m_sourceMode = normalized;
    ConfigManager::GetInstance()->setTrajectorySourceMode(normalized);
    applySourceMode();
    emit sourceModeChanged();
}

void TrajectoryDataManager::applySourceMode()
{
    clearSourceState();

    if (useDatabaseSource()) {
        m_folderPath.clear();
        refreshDatabaseSource();
        return;
    }

    disconnectDatabaseSource();
    m_sourceDescription.clear();
    m_ready = false;
    emit readyChanged();
    emit sourceDescriptionChanged();
}

void TrajectoryDataManager::clearSourceState()
{
    m_vehicleList.clear();
    m_ready = false;
}

void TrajectoryDataManager::disconnectDatabaseSource()
{
    if (m_postGisData) {
        m_postGisData->disconnectDatabase();
    }
}

void TrajectoryDataManager::releaseDatabaseConnection()
{
    disconnectDatabaseSource();
}

bool TrajectoryDataManager::connectDatabaseSource(QString& errorMessage)
{
    disconnectDatabaseSource();

    const PostGisDatabaseConfig config = ConfigManager::GetInstance()->postGisDatabaseConfig();
    if (!m_postGisData->connectDatabase(config, errorMessage)) {
        m_ready = false;
        m_sourceDescription.clear();
        emit readyChanged();
        emit sourceDescriptionChanged();
        return false;
    }

    const QList<VehicleSummary> vehicles = m_postGisData->listVehicles(errorMessage);
    if (vehicles.isEmpty()) {
        disconnectDatabaseSource();
        m_ready = false;
        m_sourceDescription.clear();
        emit readyChanged();
        emit sourceDescriptionChanged();
        return false;
    }

    m_vehicleList = vehicles;
    m_sourceDescription = config.connectionSummary();
    m_ready = true;
    emit readyChanged();
    emit sourceDescriptionChanged();
    emit scanCompleted(vehicles);
    return true;
}

void TrajectoryDataManager::refreshDatabaseSource()
{
    clearSourceState();

    QString errorMessage;
    if (!connectDatabaseSource(errorMessage)) {
        emit scanError(errorMessage);
    }
}

void TrajectoryDataManager::scanFolder(const QString& folderPath)
{
    if (useDatabaseSource()) {
        return;
    }

    const QString normalizedPath = normalizeFolderPath(folderPath);
    if (normalizedPath.isEmpty()) {
        emit scanError(QStringLiteral("请选择一个有效的文件夹路径"));
        return;
    }

    QDir dir(normalizedPath);
    if (!dir.exists()) {
        emit scanError(HANDLE_FILE_ERROR(normalizedPath, QStringLiteral("访问文件夹")));
        return;
    }

    const QFileInfo dirInfo(normalizedPath);
    if (!dirInfo.isReadable()) {
        emit scanError(HANDLE_FILE_ERROR(normalizedPath, QStringLiteral("读取文件夹")));
        return;
    }

    m_folderPath = normalizedPath;
    m_sourceDescription = normalizedPath;
    m_ready = false;
    m_vehicleList.clear();
    emit readyChanged();
    emit sourceDescriptionChanged();

    try {
        m_excelTrajectory->scanFolder(normalizedPath);
    } catch (const std::exception& e) {
        emit scanError(HANDLE_SYSTEM_ERROR(QStringLiteral("扫描文件夹"), e.what()));
    } catch (...) {
        emit scanError(HANDLE_SYSTEM_ERROR(QStringLiteral("扫描文件夹"), QStringLiteral("未知异常")));
    }
}

void TrajectoryDataManager::onExcelScanCompleted(const QList<VehicleSummary>& vehicles)
{
    m_vehicleList = vehicles;
    m_ready = !vehicles.isEmpty();
    emit readyChanged();
    emit scanCompleted(vehicles);
}

void TrajectoryDataManager::onExcelScanError(const QString& error)
{
    m_vehicleList.clear();
    m_ready = false;
    emit readyChanged();
    emit scanError(error);
}

void TrajectoryDataManager::onExcelScanProgress(int percentage)
{
    emit scanProgress(percentage);
}

void TrajectoryDataManager::onExcelLoadProgress(int percentage)
{
    emit loadProgress(percentage);
}

TrajectoryDataManager::TrajectoryLoadResult TrajectoryDataManager::loadTrajectory(
    const TrajectoryLoadRequest& request)
{
    if (request.plateNumber.trimmed().isEmpty()) {
        return TrajectoryLoadResult{{}, QStringLiteral("车牌号为空"), false};
    }

    if (!m_ready) {
        return TrajectoryLoadResult{{},
                                    useDatabaseSource()
                                        ? QStringLiteral("PostGIS 数据库未连接")
                                        : QStringLiteral("尚未加载轨迹数据源"),
                                    false};
    }

    if (useDatabaseSource()) {
        return loadTrajectoryFromDatabase(request);
    }
    return loadTrajectoryFromFolder(request);
}

TrajectoryDataManager::TrajectoryLoadResult TrajectoryDataManager::loadTrajectoryFromDatabase(
    const TrajectoryLoadRequest& request)
{
    TrajectoryLoadResult result;
    if (!m_postGisData || !m_postGisData->isConnected()) {
        result.errorMessage = QStringLiteral("PostGIS 未连接");
        return result;
    }

    emit loadProgress(10);
    result.records = m_postGisData->loadTrajectoryPoints(request.plateNumber,
                                                         result.errorMessage,
                                                         request.startDate,
                                                         request.endDate);
    emit loadProgress(80);

    result.success = !result.records.isEmpty();
    if (!result.success && result.errorMessage.isEmpty()) {
        result.errorMessage = QStringLiteral("未找到轨迹数据");
    }
    emit loadProgress(100);
    return result;
}

TrajectoryDataManager::TrajectoryLoadResult TrajectoryDataManager::loadTrajectoryFromFolder(
    const TrajectoryLoadRequest& request)
{
    TrajectoryLoadResult result;

    VehicleSummary matchedVehicle;
    bool found = false;
    for (const VehicleSummary& vehicleInfo : m_vehicleList) {
        if (vehicleInfo.plateNumber == request.plateNumber) {
            matchedVehicle = vehicleInfo;
            found = true;
            break;
        }
    }

    if (!found) {
        result.errorMessage = QStringLiteral("无法找到车辆文件: %1").arg(request.plateNumber);
        AppLogger::warn(result.errorMessage);
        return result;
    }

    const ExcelTrajectoryManager::TrajectoryLoadResult loadResult =
        m_excelTrajectory->loadTrajectory(matchedVehicle,
                                          request.plateNumber,
                                          request.startDate,
                                          request.endDate,
                                          request.hasDateRange);

    result.records = loadResult.points;
    result.errorMessage = loadResult.errorMessage;
    result.success = loadResult.success;

    if (!result.success && result.errorMessage.isEmpty()) {
        result.errorMessage = QStringLiteral("未找到车辆轨迹记录: %1").arg(request.plateNumber);
    }

    return result;
}
