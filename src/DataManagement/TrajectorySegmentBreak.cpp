#include "DataManagement/TrajectorySegmentBreak.h"

#include "Core/AppLogger.h"

#include <QVariantMap>
#include <QStringList>

namespace TrajectorySegmentBreak {

namespace {

QString formatElapsed(qint64 elapsedSeconds)
{
    if (elapsedSeconds >= 86400) {
        return QStringLiteral("%1天").arg(elapsedSeconds / 86400.0, 0, 'f', 1);
    }
    if (elapsedSeconds >= 3600) {
        return QStringLiteral("%1小时").arg(elapsedSeconds / 3600.0, 0, 'f', 1);
    }
    return QStringLiteral("%1秒").arg(elapsedSeconds);
}

QString reasonText(const Evaluation& evaluation)
{
    QStringList reasons;
    if (evaluation.distanceExceeded) {
        reasons.append(QStringLiteral("距离超限"));
    }
    if (evaluation.timeGapExceeded) {
        reasons.append(QStringLiteral("时间间隔超限"));
    }
    if (evaluation.dayChanged) {
        reasons.append(QStringLiteral("跨天"));
    }
    if (evaluation.speedExceeded) {
        reasons.append(QStringLiteral("速度超限"));
    }
    return reasons.isEmpty() ? QStringLiteral("未知") : reasons.join(QStringLiteral("+"));
}

QString kindText(const Evaluation& evaluation)
{
    if (evaluation.timeGapExceeded || evaluation.dayChanged) {
        return QStringLiteral("行程断档");
    }
    if (evaluation.distanceExceeded || evaluation.speedExceeded) {
        return QStringLiteral("GPS瞬跳");
    }
    return QStringLiteral("未知");
}

} // namespace

Evaluation evaluate(const QGeoCoordinate& previousCoordinate,
                    const QDateTime& previousTimestamp,
                    const QGeoCoordinate& coordinate,
                    const QDateTime& timestamp)
{
    Evaluation evaluation;
    if (!previousCoordinate.isValid() || !coordinate.isValid()) {
        return evaluation;
    }

    evaluation.distanceMeters = previousCoordinate.distanceTo(coordinate);
    evaluation.distanceExceeded = evaluation.distanceMeters > kMaxDistanceMeters;

    if (!previousTimestamp.isValid() || !timestamp.isValid()) {
        return evaluation;
    }

    evaluation.elapsedSeconds = previousTimestamp.secsTo(timestamp);
    if (evaluation.elapsedSeconds > 0) {
        evaluation.speedMetersPerSecond =
            evaluation.distanceMeters / static_cast<double>(evaluation.elapsedSeconds);
    }

    evaluation.timeGapExceeded = evaluation.elapsedSeconds > kMaxGapSeconds;
    evaluation.dayChanged = previousTimestamp.date() != timestamp.date();
    if (evaluation.timeGapExceeded || evaluation.dayChanged) {
        evaluation.shouldBreak = true;
        evaluation.reason = Reason::TimeGap;
    }

    evaluation.speedExceeded = evaluation.elapsedSeconds > 0
                               && evaluation.speedMetersPerSecond > kMaxSpeedMetersPerSecond;
    if (evaluation.speedExceeded) {
        evaluation.shouldBreak = true;
        if (!evaluation.distanceExceeded && !evaluation.timeGapExceeded) {
            evaluation.reason = Reason::Speed;
        }
    }

    return evaluation;
}

void logGpsJumpAnomaly(const QString& plateNumber,
                       const Evaluation& evaluation,
                       const QGeoCoordinate& previousCoordinate,
                       const QGeoCoordinate& coordinate,
                       const QDateTime& previousTimestamp,
                       const QDateTime& timestamp)
{
    if (!evaluation.shouldBreak) {
        return;
    }
    if (!evaluation.distanceExceeded && !evaluation.timeGapExceeded && !evaluation.speedExceeded) {
        return;
    }

    const QString resolvedPlate = plateNumber.trimmed().isEmpty() ? QStringLiteral("未知") : plateNumber.trimmed();
    const QString previousTimeText = previousTimestamp.isValid()
                                         ? previousTimestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                                         : QStringLiteral("-");
    const QString currentTimeText = timestamp.isValid()
                                        ? timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                                        : QStringLiteral("-");
    const QString elapsedText = evaluation.elapsedSeconds > 0
                                    ? formatElapsed(evaluation.elapsedSeconds)
                                    : QStringLiteral("-");

    AppLogger::warn(QStringLiteral(
                        "GPS跳点异常 | 车牌=%1 | 类型=%2 | 原因=%3 | 距离=%4km | 间隔=%5(%6s) | 估算速度=%7km/h | "
                        "前点=(%8,%9)@%10 | 后点=(%11,%12)@%13")
                        .arg(resolvedPlate,
                             kindText(evaluation),
                             reasonText(evaluation),
                             QString::number(evaluation.distanceMeters / 1000.0, 'f', 2),
                             elapsedText,
                             QString::number(evaluation.elapsedSeconds),
                             QString::number(evaluation.speedMetersPerSecond * 3.6, 'f', 1),
                             QString::number(previousCoordinate.latitude(), 'f', 6),
                             QString::number(previousCoordinate.longitude(), 'f', 6),
                             previousTimeText,
                             QString::number(coordinate.latitude(), 'f', 6),
                             QString::number(coordinate.longitude(), 'f', 6),
                             currentTimeText));
}

bool isDrawableCoordinate(const TrajectoryPoint& record)
{
    if (record.longitude < -180.0 || record.longitude > 180.0 || record.latitude < -90.0
        || record.latitude > 90.0) {
        return false;
    }
    return QGeoCoordinate(record.latitude, record.longitude).isValid();
}

QDateTime pointTimestamp(const QVariant& point)
{
    const QVariantMap map = point.toMap();
    if (map.isEmpty()) {
        return {};
    }
    return map.value(QStringLiteral("timestamp")).toDateTime();
}

QString pointPlateNumber(const QVariant& point)
{
    const QVariantMap map = point.toMap();
    if (map.isEmpty()) {
        return {};
    }
    return map.value(QStringLiteral("plateNumber")).toString().trimmed();
}

QVariantMap coordinateToVariantMap(const QGeoCoordinate& coordinate)
{
    QVariantMap point;
    point.insert(QStringLiteral("latitude"), coordinate.latitude());
    point.insert(QStringLiteral("longitude"), coordinate.longitude());
    return point;
}

} // namespace TrajectorySegmentBreak
