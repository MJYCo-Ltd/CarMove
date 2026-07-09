#ifndef TRAJECTORYTYPES_H
#define TRAJECTORYTYPES_H

#include <QDate>
#include <QDateTime>
#include <QGeoCoordinate>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTime>

/**
 * @brief 单个轨迹点（GPS 采样）
 */
struct TrajectoryPoint {
    QString plateNumber;
    QString vehicleColor;
    double speed = 0.0;
    double longitude = 0.0;
    double latitude = 0.0;
    int direction = 0;
    double distance = 0.0;
    QDateTime timestamp;
    QString totalMileage;

    QGeoCoordinate coordinate() const
    {
        return QGeoCoordinate(latitude, longitude);
    }

    bool isValid() const
    {
        return !plateNumber.isEmpty() && longitude >= -180.0 && longitude <= 180.0
               && latitude >= -90.0 && latitude <= 90.0 && direction >= 0 && direction <= 360
               && speed >= 0.0 && timestamp.isValid();
    }

    bool isInChinaRange() const
    {
        return longitude >= 73.0 && longitude <= 135.0 && latitude >= 18.0 && latitude <= 54.0;
    }
};

/**
 * @brief 车辆摘要（目录/列表项，可来自 Excel 文件夹或 PostGIS）
 */
struct VehicleSummary {
    QString plateNumber;
    QString plateColor;
    qint64 vehicleId = -1;
    QDateTime firstSeenAt;
    QDateTime lastSeenAt;
    int dayCount = 0;
    qint64 totalPointCount = 0;
    QStringList sourceFilePaths;
};

/**
 * @brief 按日历日聚合的轨迹（与 PostGIS trajectory_days 表对应）
 */
struct VehicleDayTrajectory {
    QString plateNumber;
    QString plateColor;
    QDate trajectoryDate;
    qint64 trajectoryDayId = -1;
    QTime firstTime;
    QTime lastTime;
    int pointCount = 0;
    QList<TrajectoryPoint> points;
};

inline QList<VehicleDayTrajectory> groupPointsByDay(const QList<TrajectoryPoint>& points)
{
    QList<VehicleDayTrajectory> days;
    if (points.isEmpty()) {
        return days;
    }

    VehicleDayTrajectory* currentDay = nullptr;
    for (const TrajectoryPoint& point : points) {
        if (!point.timestamp.isValid()) {
            continue;
        }

        const QDate date = point.timestamp.date();
        if (currentDay == nullptr || currentDay->trajectoryDate != date) {
            VehicleDayTrajectory day;
            day.plateNumber = point.plateNumber;
            day.plateColor = point.vehicleColor;
            day.trajectoryDate = date;
            day.firstTime = point.timestamp.time();
            day.lastTime = point.timestamp.time();
            day.pointCount = 1;
            day.points.append(point);
            days.append(day);
            currentDay = &days.last();
            continue;
        }

        currentDay->points.append(point);
        currentDay->pointCount = currentDay->points.size();
        currentDay->lastTime = point.timestamp.time();
    }

    return days;
}

#endif // TRAJECTORYTYPES_H
