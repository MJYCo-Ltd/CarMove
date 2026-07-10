// Private slot implementations for MainController
// Separated from MainController.cpp for maintainability
#include "UI/MainController.h"
#include "DataManagement/VehicleManager.h"
#include <QTimer>

void MainController::onDataSourceScanCompleted(const QList<TrajectoryDataManager::VehicleInfo>& vehicles)
{
    finishVehicleListLoad(vehicles);
    updateDatabaseConnectionState();
    emit currentFolderChanged();

    const QString message = m_trajectoryDataManager && m_trajectoryDataManager->useDatabaseSource()
                                ? QStringLiteral("数据库连接成功，共 %1 辆车").arg(vehicles.size())
                                : QString(QStringLiteral("成功找到 %1 辆车的数据")).arg(vehicles.size());
    emit folderScanned(true, message);
}

void MainController::onDataSourceScanError(const QString& error)
{
    m_isLoading = false;
    m_loadingMessage.clear();
    m_databaseStatus = error;
    updateDatabaseConnectionState();
    emit loadingChanged();
    emit loadingMessageChanged();
    emit folderScanned(false, error);
    emit errorOccurred(m_trajectoryDataManager && m_trajectoryDataManager->useDatabaseSource()
                           ? error
                           : QString(QStringLiteral("文件夹扫描错误: %1")).arg(error));
}

void MainController::onDataSourceScanProgress(int percentage)
{
    m_loadingMessage = m_trajectoryDataManager && m_trajectoryDataManager->useDatabaseSource()
                           ? QStringLiteral("正在连接数据库... %1%").arg(percentage)
                           : QStringLiteral("正在扫描文件夹... %1%").arg(percentage);
    emit loadingMessageChanged();
    emit loadingProgress(percentage);
}

void MainController::onDataSourceReadyChanged()
{
    updateDatabaseConnectionState();
    emit currentFolderChanged();
}

void MainController::onVehicleTrajectoryLoaded(const QString& plateNumber,
                                               const QList<TrajectoryPoint>& trajectory)
{
    const bool captureMode = m_captureTrajectoryPending;
    if (captureMode) {
        m_captureTrajectoryPending = false;
        m_isLoading = false;
        m_loadingMessage.clear();
        emit loadingChanged();
        emit loadingMessageChanged();
        const bool success = trajectory.size() >= 2;
        const int pointCount = trajectory.size();
        // 推迟到事件循环，避免 QML processNext 重入 loadTrajectoryForCapture
        QTimer::singleShot(0, this, [this, success, pointCount]() {
            emit captureTrajectoryReady(success, pointCount);
        });
        // 批量截图只需原始轨迹绘制，跳过时间轴分段/GPS 跳点日志（会阻塞 UI 线程）
        return;
    }

    if (plateNumber != m_selectedVehicle) {
        return;
    }

    syncTimelineFromVehicleManager();

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

    if (!trajectory.isEmpty()) {
        if (m_timelineManager && m_timelineManager->segmentCount() > 0) {
            m_timelineManager->seekToTime(m_timelineManager->segmentStartTime(0));
        } else if (m_timelineManager) {
            m_timelineManager->seekToProgress(0.0);
        }
    }
}

void MainController::onTrajectoryConverted(const QString& plateNumber,
                                           const QList<TrajectoryPoint>& /*convertedTrajectory*/)
{
    if (plateNumber != m_selectedVehicle) {
        return;
    }

    syncTimelineFromVehicleManager();
    emit trajectoryConverted();

    if (m_timelineManager && m_timelineManager->segmentCount() > 0) {
        m_timelineManager->seekToTime(m_timelineManager->segmentStartTime(0));
    } else if (m_timelineManager) {
        if (m_timelineManager->currentTime().isValid()) {
            m_timelineManager->seekToTime(m_timelineManager->currentTime());
        } else {
            m_timelineManager->seekToProgress(0.0);
        }
    }
}

void MainController::onVehicleLoadingProgress(int percentage)
{
    m_loadingMessage = QString("正在加载轨迹数据... %1%").arg(percentage);
    emit loadingMessageChanged();
    emit loadingProgress(percentage);
}
