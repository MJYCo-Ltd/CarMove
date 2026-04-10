// Private slot implementations for MainController
// Separated from MainController.cpp for maintainability
#include "MainController.h"
#include "VehicleManager.h"
#include "VehicleDataModel.h"
#include "VehicleAnimationEngine.h"
#include <QDebug>

void MainController::onFolderScanCompleted(const QList<FolderScanner::VehicleInfo>& vehicles)
{
    m_vehicleInfoList = vehicles;
    m_vehicleList.clear();

    for (const auto& vehicle : vehicles)
        m_vehicleList.append(vehicle.plateNumber);

    m_vehicleManager->setVehicleList(vehicles);
    updateFilteredVehicleList();

    m_isLoading = false;
    m_loadingMessage = "";
    emit loadingChanged();
    emit loadingMessageChanged();
    emit vehicleListChanged();
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
    if (plateNumber != m_selectedVehicle) return;

    setupVehicleDataModel();
    updateTimeRange();

    if (!trajectory.isEmpty()) {
        QDateTime firstTime = trajectory.first().timestamp;
        QDateTime lastTime  = trajectory.last().timestamp;
        qint64 totalDays  = firstTime.daysTo(lastTime);
        qint64 totalHours = firstTime.secsTo(lastTime) / 3600;

        QString spanInfo;
        if      (totalDays > 365) spanInfo = QString("跨度 %1 年").arg(totalDays / 365);
        else if (totalDays > 30)  spanInfo = QString("跨度 %1 个月").arg(totalDays / 30);
        else if (totalDays > 7)   spanInfo = QString("跨度 %1 周").arg(totalDays / 7);
        else if (totalDays > 0)   spanInfo = QString("跨度 %1 天").arg(totalDays);
        else                      spanInfo = QString("跨度 %1 小时").arg(totalHours);

        emit trajectoryLoaded(true, QString("成功加载 %1 个轨迹点，%2").arg(trajectory.size()).arg(spanInfo));
    } else {
        emit trajectoryLoaded(true, "成功加载轨迹数据");
    }

    m_isLoading = false;
    m_loadingMessage = "";
    emit loadingChanged();
    emit loadingMessageChanged();

    if (m_animationEngine && !trajectory.isEmpty()) {
        m_animationEngine->setVehicleModel(m_vehicleDataModel);
        m_animationEngine->stop();
        m_animationEngine->seekToProgress(0.0);
        m_currentTime = m_startTime;
        emit currentTimeChanged();
    }
}

void MainController::onTrajectoryConverted(const QString& plateNumber,
                                           const QList<ExcelDataReader::VehicleRecord>& /*convertedTrajectory*/)
{
    if (plateNumber != m_selectedVehicle) return;

    setupVehicleDataModel();
    updateTimeRange();
    emit trajectoryConverted();

    if (m_animationEngine)
        m_animationEngine->updateVehiclePositions();
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
    if (wasPlaying != m_isPlaying)
        emit playbackStateChanged();
}

void MainController::onVehiclePositionUpdate(const QString& plateNumber,
                                             const QGeoCoordinate& position,
                                             int direction, double speed)
{
    emit vehiclePositionUpdated(plateNumber, position, direction, speed);
}
