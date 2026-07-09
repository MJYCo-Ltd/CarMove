#ifndef TRAJECTORYSEGMENTBREAK_H
#define TRAJECTORYSEGMENTBREAK_H

#include "Domain/TrajectoryTypes.h"

#include <QDateTime>
#include <QGeoCoordinate>
#include <QVariant>
#include <QString>

namespace TrajectorySegmentBreak {

constexpr double kMaxDistanceMeters = 5000.0;
constexpr qint64 kMaxGapSeconds = 2 * 3600;
constexpr double kMaxSpeedMetersPerSecond = 42.0; // ~150 km/h

enum class Reason {
    None,
    Distance,
    TimeGap,
    Speed,
};

struct Evaluation {
    bool shouldBreak = false;
    Reason reason = Reason::None;
    bool distanceExceeded = false;
    bool timeGapExceeded = false;
    bool dayChanged = false;
    bool speedExceeded = false;
    double distanceMeters = 0.0;
    qint64 elapsedSeconds = 0;
    double speedMetersPerSecond = 0.0;
};

Evaluation evaluate(const QGeoCoordinate& previousCoordinate,
                    const QDateTime& previousTimestamp,
                    const QGeoCoordinate& coordinate,
                    const QDateTime& timestamp);

void logGpsJumpAnomaly(const QString& plateNumber,
                       const Evaluation& evaluation,
                       const QGeoCoordinate& previousCoordinate,
                       const QGeoCoordinate& coordinate,
                       const QDateTime& previousTimestamp,
                       const QDateTime& timestamp);

bool isDrawableCoordinate(const TrajectoryPoint& record);

QDateTime pointTimestamp(const QVariant& point);
QString pointPlateNumber(const QVariant& point);

QVariantMap coordinateToVariantMap(const QGeoCoordinate& coordinate);

} // namespace TrajectorySegmentBreak

#endif // TRAJECTORYSEGMENTBREAK_H
