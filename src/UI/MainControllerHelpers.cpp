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

        const QDateTime timestamp = TrajectorySegmentBreak::pointTimestamp(point);
        const TrajectorySegmentBreak::Evaluation breakEvaluation =
            hasPrevious ? TrajectorySegmentBreak::evaluate(previousCoordinate, previousTimestamp, coordinate, timestamp)
                        : TrajectorySegmentBreak::Evaluation{};
        const bool shouldBreak = breakEvaluation.shouldBreak;

        if (shouldBreak) {
            const QString plateNumber = TrajectorySegmentBreak::pointPlateNumber(point);
            TrajectorySegmentBreak::logGpsJumpAnomaly(plateNumber.isEmpty() ? m_selectedVehicle : plateNumber,
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

int MainController::trajectoryDisplaySegmentCount()
{
    return m_timelineManager ? m_timelineManager->displaySegmentCount() : 0;
}

QVariantList MainController::trajectoryDisplaySegmentPath(int segmentIndex) const
{
    return m_timelineManager ? m_timelineManager->displaySegmentPath(segmentIndex) : QVariantList{};
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
