#ifndef VEHICLEANIMATIONENGINE_H
#define VEHICLEANIMATIONENGINE_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QGeoCoordinate>
#include <QElapsedTimer>
#include <QHash>
#include "VehicleDataModel.h"

class VehicleAnimationEngine : public QObject
{
    Q_OBJECT
    
public:
    enum PlaybackState { Stopped, Playing, Paused };
    
    explicit VehicleAnimationEngine(QObject *parent = nullptr);
    
    void setVehicleModel(VehicleDataModel* model);
    void setPlaybackSpeed(double multiplier);
    void setCurrentTime(const QDateTime& time);
    void seekToProgress(double progress);  // 0.0-1.0 进度条拖动支持
    void setDraggingMode(bool isDragging);  // 设置拖动模式以优化性能
    void updateVehiclePositions();  // Public method to trigger position updates
    
    // Performance optimization methods
    void setAnimationFrameRate(int fps) { m_targetFps = fps; updateTimerInterval(); }
    void setInterpolationEnabled(bool enabled) { m_interpolationEnabled = enabled; }
    void setPositionCacheSize(int size) { m_maxCacheSize = size; }
    
public slots:
    void play();
    void pause();
    void stop();
    void seekToTime(const QDateTime& time);
    void onTimeSliderDragged(double progress);  // 处理时间条拖动
    
signals:
    void vehiclePositionUpdated(const QString& plateNumber, 
                               const QGeoCoordinate& position, 
                               int direction, double speed);
    void playbackStateChanged(PlaybackState state);
    void currentTimeChanged(const QDateTime& time);
    void progressChanged(double progress);  // 通知UI更新进度条位置
    
private slots:
    void updateAnimation();
    void cleanupCache();
    
private:
    // Core animation methods
    QGeoCoordinate interpolatePosition(const QGeoCoordinate& start,
                                     const QGeoCoordinate& end,
                                     double ratio) const;
    int interpolateDirection(int startDir, int endDir, double ratio) const;
    VehicleDataModel::VehicleState interpolateVehicleState(double progress) const;
    
    // Performance optimization methods
    void updateTimerInterval();
    bool shouldUpdatePosition(const QString& plateNumber, const QGeoCoordinate& newPos);
    void cacheVehicleState(const QString& plateNumber, const VehicleDataModel::VehicleState& state);
    VehicleDataModel::VehicleState getCachedVehicleState(const QString& plateNumber);
    
    // Core members
    VehicleDataModel* m_vehicleModel;
    QTimer* m_animationTimer;
    QTimer* m_cacheCleanupTimer;
    PlaybackState m_playbackState;
    double m_playbackSpeed;
    QDateTime m_currentTime;
    QDateTime m_startTime;
    QDateTime m_endTime;
    double m_currentProgress;
    bool m_isDragging;
    
    // Performance optimization members
    int m_targetFps = 30; // Target frame rate
    bool m_interpolationEnabled = true;
    int m_maxCacheSize = 1000;
    QElapsedTimer m_frameTimer;
    qint64 m_lastFrameTime = 0;
    
    // Position caching for performance
    QHash<QString, VehicleDataModel::VehicleState> m_vehicleStateCache;
    QHash<QString, QGeoCoordinate> m_lastKnownPositions;
    
    // Animation smoothing
    static constexpr double MIN_POSITION_CHANGE = 0.00001; // ~1 meter
    static constexpr double MIN_DIRECTION_CHANGE = 5.0; // 5 degrees
};

#endif // VEHICLEANIMATIONENGINE_H