// Private helper implementations for MainController
// Separated from MainController.cpp for maintainability
#include "UI/MainController.h"
#include "Core/AppLogger.h"
#include "DataManagement/TrajectorySegmentBreak.h"
#include "DataManagement/VehicleManager.h"
#include "Map/CoordinateConverter.h"
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
#include <utility>

namespace {

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

QList<QList<TrajectoryPoint>> segmentTrajectoryRecords(const QList<TrajectoryPoint>& records,
                                                       const QString& plateNumber,
                                                       bool logJumpAnomalies)
{
    QList<QList<TrajectoryPoint>> segments;
    QList<TrajectoryPoint> currentSegment;

    QGeoCoordinate previousCoordinate;
    QDateTime previousTimestamp;
    bool hasPrevious = false;

    auto flushSegment = [&]() {
        if (currentSegment.size() >= 2) {
            segments.append(std::move(currentSegment));
        }
        currentSegment = QList<TrajectoryPoint>();
    };

    for (const TrajectoryPoint& record : records) {
        if (!TrajectorySegmentBreak::isDrawableCoordinate(record)) {
            continue;
        }

        const QGeoCoordinate coordinate = record.coordinate();
        if (!coordinate.isValid()) {
            continue;
        }

        const TrajectorySegmentBreak::Evaluation breakEvaluation =
            hasPrevious ? TrajectorySegmentBreak::evaluate(previousCoordinate,
                                                           previousTimestamp,
                                                           coordinate,
                                                           record.timestamp)
                        : TrajectorySegmentBreak::Evaluation{};

        if (breakEvaluation.shouldBreak) {
            if (logJumpAnomalies) {
                const QString resolvedPlate =
                    record.plateNumber.trimmed().isEmpty() ? plateNumber : record.plateNumber.trimmed();
                TrajectorySegmentBreak::logGpsJumpAnomaly(resolvedPlate,
                                                          breakEvaluation,
                                                          previousCoordinate,
                                                          coordinate,
                                                          previousTimestamp,
                                                          record.timestamp);
            }
            flushSegment();
        }

        currentSegment.append(record);
        previousCoordinate = coordinate;
        previousTimestamp = record.timestamp;
        hasPrevious = true;
    }

    flushSegment();
    return segments;
}

QVariantList polylinePathFromRecords(const QList<TrajectoryPoint>& records)
{
    QVariantList out;
    out.reserve(records.size());
    for (const TrajectoryPoint& record : records) {
        const QGeoCoordinate coordinate = record.coordinate();
        if (coordinate.isValid()) {
            out.append(TrajectorySegmentBreak::coordinateToVariantMap(coordinate));
        }
    }
    return out;
}

QVariantList segmentPolylinePathsFromRecords(const QList<TrajectoryPoint>& records,
                                             const QString& plateNumber,
                                             bool logJumpAnomalies)
{
    QVariantList paths;
    const QList<QList<TrajectoryPoint>> segments =
        segmentTrajectoryRecords(records, plateNumber, logJumpAnomalies);
    paths.reserve(segments.size());
    for (const QList<TrajectoryPoint>& segment : segments) {
        const QVariantList polyline = polylinePathFromRecords(segment);
        if (polyline.size() >= 2) {
            appendNestedSegment(paths, polyline);
        }
    }
    return paths;
}

QGeoPath geoPathFromRecords(const QList<TrajectoryPoint>& records, const QGeoCoordinate& targetArea)
{
    QGeoPath path;
    for (const TrajectoryPoint& record : records) {
        const QGeoCoordinate coordinate = record.coordinate();
        if (coordinate.isValid()) {
            path.addCoordinate(coordinate);
        }
    }
    if (targetArea.isValid()) {
        path.addCoordinate(targetArea);
    }
    return path;
}

} // namespace

const QList<TrajectoryPoint>& MainController::activeTrajectoryRecords() const
{
    static const QList<TrajectoryPoint> kEmpty;
    if (!m_vehicleManager) {
        return kEmpty;
    }
    return m_coordinateConversionEnabled ? m_vehicleManager->convertedTrajectoryRef()
                                         : m_vehicleManager->currentTrajectoryRef();
}

int MainController::activeTrajectoryPointCount() const
{
    return activeTrajectoryRecords().size();
}

QVariantList MainController::trajectoryDisplayPolylinePaths() const
{
    if (m_timelineManager) {
        const int segmentCount = m_timelineManager->displaySegmentCount();
        if (segmentCount > 0) {
            QVariantList paths;
            paths.reserve(segmentCount);
            for (int i = 0; i < segmentCount; ++i) {
                const QVariantList polyline = m_timelineManager->displaySegmentPath(i);
                if (polyline.size() >= 2) {
                    appendNestedSegment(paths, polyline);
                }
            }
            if (!paths.isEmpty()) {
                return paths;
            }
        }
    }

    const QList<TrajectoryPoint>& records = activeTrajectoryRecords();
    if (records.isEmpty()) {
        return {};
    }
    return segmentPolylinePathsFromRecords(records, m_selectedVehicle, true);
}

QVariant MainController::trajectoryDisplayViewportShape() const
{
    const QList<TrajectoryPoint>& records = activeTrajectoryRecords();
    if (records.isEmpty()) {
        return {};
    }
    return QVariant::fromValue(geoPathFromRecords(records, targetAreaMapCoordinate()));
}

QVariantMap MainController::trajectoryDisplayStartMarker() const
{
    const QList<TrajectoryPoint>& records = activeTrajectoryRecords();
    if (records.isEmpty()) {
        return {};
    }

    const TrajectoryPoint& first = records.first();
    const QGeoCoordinate coordinate = first.coordinate();
    if (!coordinate.isValid()) {
        return {};
    }

    QVariantMap result;
    result.insert(QStringLiteral("coordinate"), QVariant::fromValue(coordinate));
    result.insert(QStringLiteral("direction"), first.direction);
    result.insert(QStringLiteral("speed"), first.speed);
    return result;
}

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

void MainController::resetTrajectoryTimeline()
{
    if (m_timelineManager) {
        m_timelineManager->resetTimeline();
    }
}

void MainController::syncTimelineFromVehicleManager()
{
    if (!m_vehicleManager || !m_timelineManager) {
        return;
    }

    const auto trajectory = m_coordinateConversionEnabled ? m_vehicleManager->getConvertedTrajectory()
                                                          : m_vehicleManager->getCurrentTrajectory();

    m_timelineManager->setSelectedVehicle(m_selectedVehicle);
    m_timelineManager->applyTrajectory(trajectory);
}

QVariantMap MainController::vehicleRecordToVariant(const TrajectoryPoint& record) const
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

int MainController::targetAreaVisitCountForPlate(const QString& plateNumber) const
{
    constexpr double kTargetAreaRadiusMeters = 1000.0;
    return calculateTargetAreaVisitCount(plateNumber, m_targetAreaLatitude, m_targetAreaLongitude,
                                         kTargetAreaRadiusMeters);
}

QGeoCoordinate MainController::targetAreaMapCoordinate() const
{
    QGeoCoordinate coord(m_targetAreaLatitude, m_targetAreaLongitude);
    if (!coord.isValid())
        return {};
    if (m_coordinateConversionEnabled)
        return CoordinateConverter::wgs84ToGcj02(coord);
    return coord;
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
            out.append(TrajectorySegmentBreak::coordinateToVariantMap(c));
    }
    return out;
}

int MainController::trajectorySegmentCount() const
{
    return m_timelineManager ? m_timelineManager->segmentCount() : 0;
}

QDateTime MainController::trajectorySegmentStartTime(int segmentIndex) const
{
    return m_timelineManager ? m_timelineManager->segmentStartTime(segmentIndex) : QDateTime{};
}

QDateTime MainController::trajectorySegmentEndTime(int segmentIndex) const
{
    return m_timelineManager ? m_timelineManager->segmentEndTime(segmentIndex) : QDateTime{};
}

int MainController::trajectoryActiveSegmentIndex() const
{
    return m_timelineManager ? m_timelineManager->activeSegmentIndex() : -1;
}

double MainController::trajectorySegmentLocalProgress(int segmentIndex) const
{
    return m_timelineManager ? m_timelineManager->segmentLocalProgress(segmentIndex) : 0.0;
}

void MainController::seekTrajectorySegment(int segmentIndex, double localProgress)
{
    if (m_timelineManager) {
        m_timelineManager->seekSegment(segmentIndex, localProgress);
    }
}

QVariant MainController::geoPathForViewport(const QVariant& trajectoryPoints) const
{
    QGeoPath path;
    for (const QVariant& v : trajectoryPoints.toList()) {
        const QGeoCoordinate c = trajectoryPointToCoordinate(v);
        if (c.isValid())
            path.addCoordinate(c);
    }

    const QGeoCoordinate target = targetAreaMapCoordinate();
    if (target.isValid())
        path.addCoordinate(target);

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

    const QList<TrajectoryPoint> trajectory =
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
        const TrajectoryPoint& record = trajectory.at(i);
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

    const TrajectoryPoint& nearest = trajectory.at(nearestIndex);
    if (!nearest.timestamp.isValid())
        return false;

    if (m_timelineManager) {
        m_timelineManager->seekToTime(nearest.timestamp);
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
