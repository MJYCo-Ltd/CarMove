#ifndef TRAJECTORYTIMELINEMANAGER_H
#define TRAJECTORYTIMELINEMANAGER_H

#include "DataManagement/TrajectoryTimeIndex.h"
#include "Domain/TrajectoryTypes.h"

#include <QDateTime>
#include <QGeoCoordinate>
#include <QObject>
#include <QVariantList>

/// 轨迹时间轴：时间范围、当前时刻、分段元数据与地图折线段。
class TrajectoryTimelineManager : public QObject
{
    Q_OBJECT

public:
    struct SegmentMeta {
        QDateTime startTime;
        QDateTime endTime;
    };

    explicit TrajectoryTimelineManager(QObject* parent = nullptr);

    void setSelectedVehicle(const QString& plateNumber);
    void applyTrajectory(const QList<TrajectoryPoint>& trajectory);
    void clearTimeline();

    QDateTime startTime() const { return m_startTime; }
    QDateTime endTime() const { return m_endTime; }
    QDateTime currentTime() const { return m_currentTime; }
    bool spansMultipleDays() const;

    void resetTimeline();
    void seekToTime(const QDateTime& time);
    void seekToProgress(double progress);

    int segmentCount() const { return m_segmentMeta.size(); }
    QDateTime segmentStartTime(int segmentIndex) const;
    QDateTime segmentEndTime(int segmentIndex) const;
    int activeSegmentIndex() const;
    double segmentLocalProgress(int segmentIndex) const;
    void seekSegment(int segmentIndex, double localProgress);

    int displaySegmentCount();
    QVariantList displaySegmentPath(int segmentIndex) const;

    void invalidateSegments();

signals:
    void timeRangeChanged();
    void currentTimeChanged();
    void segmentsChanged();
    void vehiclePositionUpdated(const QString& plateNumber,
                                const QGeoCoordinate& position,
                                int direction,
                                double speed);

private:
    void syncTimeRangeFromIndex();
    void publishVehiclePositionAtCurrentTime();
    void rebuildSegments(const QList<TrajectoryPoint>& trajectory);

    TrajectoryTimeIndex m_index;
    QString m_selectedVehicle;
    QDateTime m_startTime;
    QDateTime m_endTime;
    QDateTime m_currentTime;
    QList<SegmentMeta> m_segmentMeta;
    QVariantList m_displaySegments;
    bool m_segmentsNeedRebuild = true;
    bool m_isRebuildingSegments = false;
};

#endif // TRAJECTORYTIMELINEMANAGER_H
