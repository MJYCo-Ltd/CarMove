// Private helper implementations for MainController
// Separated from MainController.cpp for maintainability
#include "MainController.h"
#include "AppLogger.h"
#include "VehicleManager.h"
#include "VehicleDataModel.h"
#include "VehicleAnimationEngine.h"
#include "PlaybackControl.h"
#include <QGeoCoordinate>
#include <QGeoPath>
#include <QStandardPaths>
#include <QVariantMap>
#include <QDir>
#include <QDateTime>
#include <QStringList>
#include <QMetaType>
#include <QtGlobal>
#include <limits>

namespace {

constexpr double kTrajectorySegmentMaxDistanceMeters = 5000.0;
constexpr qint64 kTrajectorySegmentMaxGapSeconds = 2 * 3600;
constexpr double kTrajectorySegmentMaxSpeedMetersPerSecond = 42.0; // ~150 km/h

enum class TrajectorySegmentBreakReason {
    None,
    Distance,
    TimeGap,
    Speed,
};

struct TrajectorySegmentBreakEvaluation {
    bool shouldBreak = false;
    TrajectorySegmentBreakReason reason = TrajectorySegmentBreakReason::None;
    bool distanceExceeded = false;
    bool timeGapExceeded = false;
    bool dayChanged = false;
    bool speedExceeded = false;
    double distanceMeters = 0.0;
    qint64 elapsedSeconds = 0;
    double speedMetersPerSecond = 0.0;
};

QDateTime trajectoryPointTimestamp(const QVariant& point)
{
    const QVariantMap map = point.toMap();
    if (map.isEmpty()) {
        return {};
    }
    return map.value(QStringLiteral("timestamp")).toDateTime();
}

QString trajectoryPointPlateNumber(const QVariant& point)
{
    const QVariantMap map = point.toMap();
    if (map.isEmpty()) {
        return {};
    }
    return map.value(QStringLiteral("plateNumber")).toString().trimmed();
}

TrajectorySegmentBreakEvaluation evaluateTrajectorySegmentBreak(const QGeoCoordinate& previousCoordinate,
                                                                const QDateTime& previousTimestamp,
                                                                const QGeoCoordinate& coordinate,
                                                                const QDateTime& timestamp)
{
    TrajectorySegmentBreakEvaluation evaluation;
    if (!previousCoordinate.isValid() || !coordinate.isValid()) {
        return evaluation;
    }

    evaluation.distanceMeters = previousCoordinate.distanceTo(coordinate);
    evaluation.distanceExceeded = evaluation.distanceMeters > kTrajectorySegmentMaxDistanceMeters;

    if (!previousTimestamp.isValid() || !timestamp.isValid()) {
        return evaluation;
    }

    evaluation.elapsedSeconds = previousTimestamp.secsTo(timestamp);
    if (evaluation.elapsedSeconds > 0) {
        evaluation.speedMetersPerSecond =
            evaluation.distanceMeters / static_cast<double>(evaluation.elapsedSeconds);
    }

    evaluation.timeGapExceeded = evaluation.elapsedSeconds > kTrajectorySegmentMaxGapSeconds;
    evaluation.dayChanged = previousTimestamp.date() != timestamp.date();
    if (evaluation.timeGapExceeded || evaluation.dayChanged) {
        evaluation.shouldBreak = true;
        evaluation.reason = TrajectorySegmentBreakReason::TimeGap;
    }

    evaluation.speedExceeded = evaluation.elapsedSeconds > 0
        && evaluation.speedMetersPerSecond > kTrajectorySegmentMaxSpeedMetersPerSecond;
    if (evaluation.speedExceeded) {
        evaluation.shouldBreak = true;
        if (!evaluation.distanceExceeded && !evaluation.timeGapExceeded) {
            evaluation.reason = TrajectorySegmentBreakReason::Speed;
        }
    }

    return evaluation;
}

QString formatTrajectoryBreakElapsed(qint64 elapsedSeconds)
{
    if (elapsedSeconds >= 86400) {
        return QStringLiteral("%1天").arg(elapsedSeconds / 86400.0, 0, 'f', 1);
    }
    if (elapsedSeconds >= 3600) {
        return QStringLiteral("%1小时").arg(elapsedSeconds / 3600.0, 0, 'f', 1);
    }
    return QStringLiteral("%1秒").arg(elapsedSeconds);
}

QString trajectoryBreakReasonText(const TrajectorySegmentBreakEvaluation& evaluation)
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

QString trajectoryBreakKindText(const TrajectorySegmentBreakEvaluation& evaluation)
{
    if (evaluation.timeGapExceeded || evaluation.dayChanged) {
        return QStringLiteral("行程断档");
    }
    if (evaluation.distanceExceeded || evaluation.speedExceeded) {
        return QStringLiteral("GPS瞬跳");
    }
    return QStringLiteral("未知");
}

void logTrajectoryGpsJumpAnomaly(const QString& plateNumber,
                                 const TrajectorySegmentBreakEvaluation& evaluation,
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

    const QString reasonText = trajectoryBreakReasonText(evaluation);
    const QString kindText = trajectoryBreakKindText(evaluation);
    const QString resolvedPlate = plateNumber.trimmed().isEmpty() ? QStringLiteral("未知") : plateNumber.trimmed();
    const QString previousTimeText = previousTimestamp.isValid()
                                       ? previousTimestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                                       : QStringLiteral("-");
    const QString currentTimeText = timestamp.isValid()
                                        ? timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                                        : QStringLiteral("-");
    const QString elapsedText = evaluation.elapsedSeconds > 0
                                    ? formatTrajectoryBreakElapsed(evaluation.elapsedSeconds)
                                    : QStringLiteral("-");

    AppLogger::warn(QStringLiteral(
                        "GPS跳点异常 | 车牌=%1 | 类型=%2 | 原因=%3 | 距离=%4km | 间隔=%5(%6s) | 估算速度=%7km/h | "
                        "前点=(%8,%9)@%10 | 后点=(%11,%12)@%13")
                        .arg(resolvedPlate,
                             kindText,
                             reasonText,
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

bool isDrawableTrajectoryCoordinate(const ExcelDataReader::VehicleRecord& record)
{
    if (record.longitude < -180.0 || record.longitude > 180.0 || record.latitude < -90.0
        || record.latitude > 90.0) {
        return false;
    }
    return QGeoCoordinate(record.latitude, record.longitude).isValid();
}
QVariantMap coordinateToVariantMap(const QGeoCoordinate& coordinate)
{
    QVariantMap point;
    point.insert(QStringLiteral("latitude"), coordinate.latitude());
    point.insert(QStringLiteral("longitude"), coordinate.longitude());
    return point;
}

void appendNestedSegment(QVariantList& target, const QVariantList& segment)
{
    target.append(QVariant::fromValue(segment));
}

QVariantList nestedSegmentToList(const QVariant& segmentVariant)
{
    if (!segmentVariant.isValid()) {
        return {};
    }
    if (segmentVariant.typeId() == QMetaType::QVariantList) {
        return segmentVariant.toList();
    }
    if (segmentVariant.canConvert<QVariantList>()) {
        return segmentVariant.toList();
    }
    return {};
}

} // namespace

void MainController::updateFilteredVehicleList()
{
    if (m_searchText.isEmpty()) {
        m_filteredVehicleList = m_vehicleList;
    } else {
        m_filteredVehicleList.clear();
        for (const QString& vehicle : m_vehicleList) {
            if (vehicle.contains(m_searchText, Qt::CaseInsensitive))
                m_filteredVehicleList.append(vehicle);
        }
    }
    emit filteredVehicleListChanged();
}

void MainController::updateTimeRange()
{
    if (m_playbackControl)
        m_playbackControl->notifyModelTimeRangeUpdated();
}

void MainController::setupVehicleDataModel()
{
    if (!m_vehicleManager || !m_vehicleDataModel) return;

    auto trajectory = m_coordinateConversionEnabled
                      ? m_vehicleManager->getConvertedTrajectory()
                      : m_vehicleManager->getCurrentTrajectory();

    m_vehicleDataModel->setVehicleData(trajectory);

    if (m_animationEngine)
        m_animationEngine->setVehicleModel(m_vehicleDataModel);
}

QVariantMap MainController::vehicleRecordToVariant(const ExcelDataReader::VehicleRecord& record) const
{
    QVariantMap result;
    result["plateNumber"]  = record.plateNumber;
    result["vehicleColor"] = record.vehicleColor;
    result["speed"]        = record.speed;
    result["longitude"]    = record.longitude;
    result["latitude"]     = record.latitude;
    result["direction"]    = record.direction;
    result["distance"]     = record.distance;
    result["timestamp"]    = record.timestamp;
    result["totalMileage"] = record.totalMileage;
    result["coordinate"]   = QVariant::fromValue(QGeoCoordinate(record.latitude, record.longitude));
    return result;
}

int MainController::calculateTargetAreaVisitCount(const QString& plateNumber, double targetLat, double targetLon, double radiusMeters) const
{
    if (!m_vehicleManager || m_vehicleManager->getSelectedVehicle() != plateNumber)
        return 0;

    const auto trajectory = m_vehicleManager->getCurrentTrajectory();
    if (trajectory.isEmpty())
        return 0;

    const QGeoCoordinate target(targetLat, targetLon);
    int visitCount = 0;
    bool prevInside = false;
    bool hasPrev = false;

    for (const auto& record : trajectory) {
        const QGeoCoordinate coord(record.latitude, record.longitude);
        const bool inside = target.distanceTo(coord) <= radiusMeters;
        if (inside && (!hasPrev || !prevInside))
            ++visitCount;
        prevInside = inside;
        hasPrev = true;
    }
    return visitCount;
}

QGeoCoordinate MainController::trajectoryPointToCoordinate(const QVariant& point) const
{
    if (!point.isValid())
        return {};

    if (point.canConvert<QGeoCoordinate>()) {
        const QGeoCoordinate c = point.value<QGeoCoordinate>();
        if (c.isValid())
            return c;
    }

    const QVariantMap m = point.toMap();
    if (m.isEmpty())
        return {};

    if (m.contains(QStringLiteral("coordinate"))) {
        const QVariant cv = m.value(QStringLiteral("coordinate"));
        if (cv.canConvert<QGeoCoordinate>()) {
            const QGeoCoordinate c = cv.value<QGeoCoordinate>();
            if (c.isValid())
                return c;
        }
    }
    if (m.contains(QStringLiteral("latitude")) && m.contains(QStringLiteral("longitude"))) {
        return QGeoCoordinate(m.value(QStringLiteral("latitude")).toDouble(),
                              m.value(QStringLiteral("longitude")).toDouble());
    }
    return {};
}

QString MainController::colorHexForPlate(const QString& plateNumber) const
{
    static const QStringList colors = {
        QStringLiteral("#e74c3c"), QStringLiteral("#3498db"), QStringLiteral("#2ecc71"),
        QStringLiteral("#f39c12"), QStringLiteral("#9b59b6"), QStringLiteral("#1abc9c"),
        QStringLiteral("#e67e22"), QStringLiteral("#34495e")
    };
    int hash = 0;
    const QString s = plateNumber;
    for (const QChar ch : s)
        hash = ch.unicode() + ((hash << 5) - hash);
    const int n = colors.size();
    int idx = hash % n;
    if (idx < 0)
        idx += n;
    return colors.at(idx);
}

QVariantList MainController::trajectoryPolylinePath(const QVariant& trajectoryPoints) const
{
    QVariantList out;
    const QVariantList points = trajectoryPoints.toList();
    for (const QVariant& v : points) {
        const QGeoCoordinate c = trajectoryPointToCoordinate(v);
        if (c.isValid())
            out.append(coordinateToVariantMap(c));
    }
    return out;
}

QVariantList MainController::trajectoryPointSegments(const QVariant& trajectoryPoints) const
{
    const QVariantList trajectoryPointList = trajectoryPoints.toList();
    QVariantList segments;
    QVariantList currentSegment;

    QGeoCoordinate previousCoordinate;
    QDateTime previousTimestamp;
    bool hasPrevious = false;

    auto flushSegment = [&]() {
        if (currentSegment.size() >= 2) {
            appendNestedSegment(segments, currentSegment);
        }
        currentSegment = QVariantList();
    };

    for (const QVariant& point : trajectoryPointList) {
        const QGeoCoordinate coordinate = trajectoryPointToCoordinate(point);
        if (!coordinate.isValid()) {
            continue;
        }

        const QDateTime timestamp = trajectoryPointTimestamp(point);
        const TrajectorySegmentBreakEvaluation breakEvaluation = hasPrevious
            ? evaluateTrajectorySegmentBreak(previousCoordinate, previousTimestamp, coordinate, timestamp)
            : TrajectorySegmentBreakEvaluation{};
        const bool shouldBreak = breakEvaluation.shouldBreak;

        if (shouldBreak) {
            const QString plateNumber = trajectoryPointPlateNumber(point);
            logTrajectoryGpsJumpAnomaly(plateNumber.isEmpty() ? m_selectedVehicle : plateNumber,
                                        breakEvaluation,
                                        previousCoordinate,
                                        coordinate,
                                        previousTimestamp,
                                        timestamp);
            flushSegment();
        }

        currentSegment.append(point);
        previousCoordinate = coordinate;
        previousTimestamp = timestamp;
        hasPrevious = true;
    }

    flushSegment();

    return segments;
}

QVariantList MainController::trajectorySegmentPolylinePaths(const QVariant& trajectoryPoints) const
{
    QVariantList paths;
    const QVariantList segments = trajectoryPointSegments(trajectoryPoints);
    for (const QVariant& segment : segments) {
        const QVariantList polyline = trajectoryPolylinePath(segment);
        if (polyline.size() >= 2) {
            appendNestedSegment(paths, polyline);
        }
    }
    return paths;
}

void MainController::rebuildTrajectorySegments()
{
    if (m_isRebuildingTrajectorySegments) {
        return;
    }
    m_isRebuildingTrajectorySegments = true;
    m_trajectoryDisplaySegments.clear();
    m_playbackSegmentMeta.clear();

    if (!m_vehicleManager) {
        m_segmentsNeedRebuild = false;
        m_isRebuildingTrajectorySegments = false;
        emit playbackSegmentsChanged();
        return;
    }

    const QList<ExcelDataReader::VehicleRecord> trajectory =
        m_coordinateConversionEnabled ? m_vehicleManager->getConvertedTrajectory()
                                      : m_vehicleManager->getCurrentTrajectory();

    if (trajectory.size() < 2) {
        m_segmentsNeedRebuild = false;
        m_isRebuildingTrajectorySegments = false;
        emit playbackSegmentsChanged();
        return;
    }

    QVariantList currentSegment;
    int validPointCount = 0;
    int invalidPointCount = 0;
    int breakCount = 0;

    QGeoCoordinate previousCoordinate;
    QDateTime previousTimestamp;
    QDateTime segmentStartTime;
    QDateTime segmentEndTime;
    bool hasPrevious = false;

    auto flushSegment = [&]() {
        if (currentSegment.size() >= 2 && segmentStartTime.isValid() && segmentEndTime.isValid()) {
            appendNestedSegment(m_trajectoryDisplaySegments, currentSegment);
            PlaybackSegmentMeta meta;
            meta.startTime = segmentStartTime;
            meta.endTime = segmentEndTime;
            m_playbackSegmentMeta.append(meta);
        }
        currentSegment = QVariantList();
        segmentStartTime = QDateTime();
        segmentEndTime = QDateTime();
    };

    for (const ExcelDataReader::VehicleRecord& record : trajectory) {
        if (!isDrawableTrajectoryCoordinate(record)) {
            ++invalidPointCount;
            continue;
        }
        ++validPointCount;

        const QGeoCoordinate coordinate(record.latitude, record.longitude);

        const TrajectorySegmentBreakEvaluation breakEvaluation = hasPrevious
            ? evaluateTrajectorySegmentBreak(previousCoordinate, previousTimestamp, coordinate, record.timestamp)
            : TrajectorySegmentBreakEvaluation{};
        const bool shouldBreak = breakEvaluation.shouldBreak;

        if (shouldBreak) {
            ++breakCount;
            logTrajectoryGpsJumpAnomaly(record.plateNumber.isEmpty() ? m_selectedVehicle : record.plateNumber,
                                        breakEvaluation,
                                        previousCoordinate,
                                        coordinate,
                                        previousTimestamp,
                                        record.timestamp);
            flushSegment();
        }

        if (currentSegment.isEmpty() && record.timestamp.isValid()) {
            segmentStartTime = record.timestamp;
        }
        if (record.timestamp.isValid()) {
            segmentEndTime = record.timestamp;
        }

        currentSegment.append(coordinateToVariantMap(coordinate));
        previousCoordinate = coordinate;
        previousTimestamp = record.timestamp;
        hasPrevious = true;
    }

    flushSegment();

    if (m_trajectoryDisplaySegments.isEmpty() && validPointCount > 0) {
        AppLogger::warn(QStringLiteral(
                            "轨迹绘制分段为空 | 车牌=%1 | 总点数=%2 | 有效坐标=%3 | 无效坐标=%4 | 断点=%5 | "
                            "提示=相邻点均无法形成>=2点的连续段，请检查是否存在大量孤立跳点")
                            .arg(m_selectedVehicle)
                            .arg(trajectory.size())
                            .arg(validPointCount)
                            .arg(invalidPointCount)
                            .arg(breakCount));
    }

    m_segmentsNeedRebuild = false;
    m_isRebuildingTrajectorySegments = false;
    emit playbackSegmentsChanged();
}

int MainController::trajectoryDisplaySegmentCount()
{
    if (m_segmentsNeedRebuild) {
        rebuildTrajectorySegments();
    }
    return m_trajectoryDisplaySegments.size();
}

QVariantList MainController::trajectoryDisplaySegmentPath(int segmentIndex) const
{
    if (segmentIndex < 0 || segmentIndex >= m_trajectoryDisplaySegments.size()) {
        return {};
    }
    return nestedSegmentToList(m_trajectoryDisplaySegments.at(segmentIndex));
}

void MainController::refreshPlaybackSegments()
{
    m_segmentsNeedRebuild = true;
    rebuildTrajectorySegments();
}

int MainController::playbackSegmentCount() const
{
    return m_playbackSegmentMeta.size();
}

QDateTime MainController::playbackSegmentStartTime(int segmentIndex) const
{
    if (segmentIndex < 0 || segmentIndex >= m_playbackSegmentMeta.size()) {
        return {};
    }
    return m_playbackSegmentMeta.at(segmentIndex).startTime;
}

QDateTime MainController::playbackSegmentEndTime(int segmentIndex) const
{
    if (segmentIndex < 0 || segmentIndex >= m_playbackSegmentMeta.size()) {
        return {};
    }
    return m_playbackSegmentMeta.at(segmentIndex).endTime;
}

qint64 MainController::playbackSegmentDurationMs(int segmentIndex) const
{
    if (segmentIndex < 0 || segmentIndex >= m_playbackSegmentMeta.size()) {
        return 0;
    }
    const PlaybackSegmentMeta& meta = m_playbackSegmentMeta.at(segmentIndex);
    if (!meta.startTime.isValid() || !meta.endTime.isValid()) {
        return 0;
    }
    return meta.startTime.msecsTo(meta.endTime);
}

int MainController::playbackActiveSegmentIndex() const
{
    if (m_playbackSegmentMeta.isEmpty() || !m_playbackControl) {
        return -1;
    }

    const QDateTime current = m_playbackControl->currentTime();
    for (int i = 0; i < m_playbackSegmentMeta.size(); ++i) {
        const PlaybackSegmentMeta& meta = m_playbackSegmentMeta.at(i);
        if (current >= meta.startTime && current <= meta.endTime) {
            return i;
        }
    }

    for (int i = m_playbackSegmentMeta.size() - 1; i >= 0; --i) {
        if (current >= m_playbackSegmentMeta.at(i).startTime) {
            return i;
        }
    }

    return 0;
}

double MainController::playbackSegmentLocalProgress(int segmentIndex) const
{
    if (segmentIndex < 0 || segmentIndex >= m_playbackSegmentMeta.size() || !m_playbackControl) {
        return 0.0;
    }

    const PlaybackSegmentMeta& meta = m_playbackSegmentMeta.at(segmentIndex);
    const qint64 totalMs = meta.startTime.msecsTo(meta.endTime);
    if (totalMs <= 0) {
        return 0.0;
    }

    const QDateTime current = m_playbackControl->currentTime();
    const qint64 elapsedMs = meta.startTime.msecsTo(current);
    return qBound(0.0, static_cast<double>(elapsedMs) / static_cast<double>(totalMs), 1.0);
}

void MainController::seekPlaybackSegment(int segmentIndex, double localProgress)
{
    if (segmentIndex < 0 || segmentIndex >= m_playbackSegmentMeta.size() || !m_animationEngine) {
        return;
    }

    const PlaybackSegmentMeta& meta = m_playbackSegmentMeta.at(segmentIndex);
    if (!meta.startTime.isValid() || !meta.endTime.isValid()) {
        return;
    }

    localProgress = qBound(0.0, localProgress, 1.0);
    const qint64 totalMs = meta.startTime.msecsTo(meta.endTime);
    const QDateTime target = meta.startTime.addMSecs(static_cast<qint64>(totalMs * localProgress));
    m_animationEngine->seekToTime(target);
}

QVariant MainController::geoPathFromTrajectory(const QVariant& trajectoryPoints) const
{
    QGeoPath path;
    for (const QVariant& v : trajectoryPoints.toList()) {
        const QGeoCoordinate c = trajectoryPointToCoordinate(v);
        if (c.isValid())
            path.addCoordinate(c);
    }
    return QVariant::fromValue(path);
}

QVariant MainController::geoPathForViewport(const QVariant& trajectoryPoints) const
{
    QGeoPath path;
    for (const QVariant& v : trajectoryPoints.toList()) {
        const QGeoCoordinate c = trajectoryPointToCoordinate(v);
        if (c.isValid()) {
            path.addCoordinate(c);
        }
    }

    const QGeoCoordinate target(m_targetAreaLatitude, m_targetAreaLongitude);
    if (target.isValid()) {
        path.addCoordinate(target);
    }

    return QVariant::fromValue(path);
}

QString MainController::formatRecordsTotalAmount(const QVariantList& records) const
{
    double total = 0;
    for (const QVariant& rv : records) {
        const QVariantMap m = rv.toMap();
        total += m.value(QStringLiteral("amount")).toDouble();
    }
    return QString::number(total, 'f', 2);
}

QVariantMap MainController::batchTargetAreaVisitCounts(const QVariantList& plateNumbers, double lat, double lon, double radiusMeters) const
{
    QVariantMap out;
    for (const QVariant& pv : plateNumbers) {
        const QString plate = pv.toString();
        if (!plate.isEmpty())
            out.insert(plate, calculateTargetAreaVisitCount(plate, lat, lon, radiusMeters));
    }
    return out;
}

bool MainController::isVehicleMoveBelowDistanceThreshold(const QGeoCoordinate& prevCoord, const QGeoCoordinate& newCoord, double thresholdMeters) const
{
    if (!prevCoord.isValid() || !newCoord.isValid())
        return false;
    return prevCoord.distanceTo(newCoord) < thresholdMeters;
}

bool MainController::seekVehicleToNearestTrajectoryPoint(double latitude, double longitude)
{
    if (!m_vehicleManager || m_selectedVehicle.isEmpty())
        return false;

    const QList<ExcelDataReader::VehicleRecord> trajectory =
        m_coordinateConversionEnabled ? m_vehicleManager->getConvertedTrajectory()
                                      : m_vehicleManager->getCurrentTrajectory();
    if (trajectory.isEmpty())
        return false;

    const QGeoCoordinate target(latitude, longitude);
    if (!target.isValid())
        return false;

    int nearestIndex = -1;
    double nearestDistance = std::numeric_limits<double>::max();
    for (int i = 0; i < trajectory.size(); ++i) {
        const ExcelDataReader::VehicleRecord& record = trajectory.at(i);
        const QGeoCoordinate point(record.latitude, record.longitude);
        if (!point.isValid())
            continue;

        const double distance = target.distanceTo(point);
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearestIndex = i;
        }
    }

    if (nearestIndex < 0)
        return false;

    const ExcelDataReader::VehicleRecord& nearest = trajectory.at(nearestIndex);
    if (!nearest.timestamp.isValid())
        return false;

    if (m_playbackControl) {
        if (m_playbackControl->isPlaying())
            m_playbackControl->pausePlayback();
        m_playbackControl->seekToTime(nearest.timestamp);
    } else if (m_animationEngine) {
        m_animationEngine->seekToTime(nearest.timestamp);
    }

    return true;
}

QString MainController::getDocumentsPath()
{
    QString docPath     = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString screenshotDir = docPath + "/CarMove_Screenshots";

    QDir dir;
    if (!dir.exists(screenshotDir) && !dir.mkpath(screenshotDir))
        AppLogger::warn(QStringLiteral("无法创建截图目录: %1").arg(screenshotDir));

    return docPath;
}
