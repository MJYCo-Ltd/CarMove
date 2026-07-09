#ifndef TRAJECTORYTIMEINDEX_H
#define TRAJECTORYTIMEINDEX_H

#include "Domain/TrajectoryTypes.h"

#include <QDateTime>
#include <QGeoCoordinate>
#include <QHash>
#include <QObject>

/// 轨迹点时间索引：供时间轴按时刻查询车辆位置（非列表模型）。
class TrajectoryTimeIndex : public QObject
{
    Q_OBJECT

public:
    struct VehicleSnapshot {
        QString plateNumber;
        QGeoCoordinate position;
        double speed = 0.0;
        int direction = 0;
        QDateTime timestamp;
        QString color;
    };

    explicit TrajectoryTimeIndex(QObject* parent = nullptr);

    void setTrajectoryData(const QList<TrajectoryPoint>& records);
    QList<VehicleSnapshot> vehicleSnapshotsAtTime(const QDateTime& time);
    QDateTime startTime() const { return m_startTime; }
    QDateTime endTime() const { return m_endTime; }
    void clearCache() { m_stateCache.clear(); }

private:
    void calculateTimeRange();
    void buildTimeIndex();
    void addToTimeIndex(const TrajectoryPoint& record, int index);
    QList<VehicleSnapshot> computeVehicleSnapshotsAtTime(const QDateTime& time);
    qint64 timeToKey(const QDateTime& time) const;

    QList<TrajectoryPoint> m_vehicleRecords;
    QDateTime m_startTime;
    QDateTime m_endTime;
    QHash<qint64, QList<int>> m_timeIndex;
    QHash<qint64, QList<VehicleSnapshot>> m_stateCache;
};

#endif // TRAJECTORYTIMEINDEX_H
