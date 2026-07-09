// Private slot implementations for MainController
// Separated from MainController.cpp for maintainability
#include "UI/MainController.h"
#include "DataManagement/VehicleManager.h"
#include "DataManagement/VehicleDataModel.h"
#include "Map/VehicleAnimationEngine.h"

void MainController::onFolderScanCompleted(const QList<FolderScanner::VehicleInfo>& vehicles)
{
    finishVehicleListLoad(vehicles);
    emit folderScanned(true, QString("成功找到 %1 辆车的数据").arg(vehicles.size()));
}

void MainController::onFolderScanError(const QString& error)
{
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
    const bool captureMode = m_captureTrajectoryPending;
    if (captureMode) {
        m_captureTrajectoryPending = false;
        m_isLoading = false;
        m_loadingMessage.clear();
        emit loadingChanged();
        emit loadingMessageChanged();
        emit captureTrajectoryReady(!trajectory.isEmpty(), trajectory.size());
    }

    if (plateNumber != m_selectedVehicle) {
        return;
    }

    setupVehicleDataModel();
    rebuildTrajectorySegments();
    updateTimeRange();

    if (!trajectory.isEmpty()) {
        QDateTime firstTime = trajectory.first().timestamp;
        QDateTime lastTime = trajectory.last().timestamp;
        qint64 totalDays = firstTime.daysTo(lastTime);
        qint64 totalHours = firstTime.secsTo(lastTime) / 3600;

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

        if (!captureMode) {
            emit trajectoryLoaded(true,
                                  QString("成功加载 %1 个轨迹点，%2").arg(trajectory.size()).arg(spanInfo));
        }
    } else if (!captureMode) {
        emit trajectoryLoaded(false, QStringLiteral("未找到有效轨迹点"));
    }

    if (!captureMode) {
        m_isLoading = false;
        m_loadingMessage = "";
        emit loadingChanged();
        emit loadingMessageChanged();
    }

    if (m_animationEngine && !trajectory.isEmpty()) {
        m_animationEngine->setVehicleModel(m_vehicleDataModel);
        m_animationEngine->stop();
        if (!m_playbackSegmentMeta.isEmpty()) {
            m_animationEngine->seekToTime(m_playbackSegmentMeta.first().startTime);
        } else {
            m_animationEngine->seekToProgress(0.0);
        }
    }
}

void MainController::onTrajectoryConverted(const QString& plateNumber,
                                           const QList<ExcelDataReader::VehicleRecord>& /*convertedTrajectory*/)
{
    if (plateNumber != m_selectedVehicle) return;

    setupVehicleDataModel();
    rebuildTrajectorySegments();
    updateTimeRange();
    emit trajectoryConverted();

    if (m_animationEngine) {
        if (!m_playbackSegmentMeta.isEmpty()) {
            m_animationEngine->seekToTime(m_playbackSegmentMeta.first().startTime);
        }
        m_animationEngine->updateVehiclePositions();
    }
}

void MainController::onVehicleLoadingProgress(int percentage)
{
    m_loadingMessage = QString("正在加载轨迹数据... %1%").arg(percentage);
    emit loadingMessageChanged();
    emit loadingProgress(percentage);
}

void MainController::onVehiclePositionUpdate(const QString& plateNumber,
                                             const QGeoCoordinate& position,
                                             int direction, double speed)
{
    emit vehiclePositionUpdated(plateNumber, position, direction, speed);
}
