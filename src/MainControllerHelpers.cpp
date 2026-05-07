// Private helper implementations for MainController
// Separated from MainController.cpp for maintainability
#include "MainController.h"
#include "VehicleManager.h"
#include "VehicleDataModel.h"
#include "VehicleAnimationEngine.h"
#include <QGeoCoordinate>
#include <QGeoPath>
#include <QStandardPaths>
#include <QVariantMap>
#include <QDir>

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

QVariantMap MainController::vehicleRecordToVariant(const ExcelDataReader::VehicleRecord& record)
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

QVariantList MainController::trajectoryPolylinePath(const QVariantList& trajectoryPoints) const
{
    QVariantList out;
    for (const QVariant& v : trajectoryPoints) {
        const QGeoCoordinate c = trajectoryPointToCoordinate(v);
        if (c.isValid())
            out.append(QVariant::fromValue(c));
    }
    return out;
}

QVariant MainController::geoPathFromTrajectory(const QVariantList& trajectoryPoints) const
{
    QGeoPath path;
    for (const QVariant& v : trajectoryPoints) {
        const QGeoCoordinate c = trajectoryPointToCoordinate(v);
        if (c.isValid())
            path.addCoordinate(c);
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

QString MainController::getDocumentsPath()
{
    QString docPath     = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString screenshotDir = docPath + "/CarMove_Screenshots";

    QDir dir;
    if (!dir.exists(screenshotDir) && !dir.mkpath(screenshotDir))
        qWarning() << "无法创建截图目录:" << screenshotDir;

    return docPath;
}
