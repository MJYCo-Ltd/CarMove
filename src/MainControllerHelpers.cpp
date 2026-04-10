// Private helper implementations for MainController
// Separated from MainController.cpp for maintainability
#include "MainController.h"
#include "VehicleManager.h"
#include "VehicleDataModel.h"
#include "VehicleAnimationEngine.h"
#include <QGeoCoordinate>
#include <QSet>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

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
    if (!m_vehicleDataModel) return;

    QDateTime newStart = m_vehicleDataModel->getStartTime();
    QDateTime newEnd   = m_vehicleDataModel->getEndTime();
    bool changed = false;

    if (m_startTime != newStart) { m_startTime = newStart; changed = true; }
    if (m_endTime   != newEnd)   { m_endTime   = newEnd;   changed = true; }

    if (changed) {
        emit timeRangeChanged();
        if (!m_currentTime.isValid() || m_currentTime < m_startTime || m_currentTime > m_endTime) {
            m_currentTime = m_startTime;
            emit currentTimeChanged();
        }
    }
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

int MainController::calculateVisitDays(const QString& plateNumber, double targetLat, double targetLon, double radiusMeters)
{
    if (!m_vehicleManager || m_vehicleManager->getSelectedVehicle() != plateNumber)
        return 0;

    auto trajectory = m_vehicleManager->getCurrentTrajectory();
    if (trajectory.isEmpty()) return 0;

    QGeoCoordinate target(targetLat, targetLon);
    QSet<QDate> visitDates;

    for (const auto& record : trajectory) {
        QGeoCoordinate coord(record.latitude, record.longitude);
        if (target.distanceTo(coord) <= radiusMeters)
            visitDates.insert(record.timestamp.date());
    }

    return visitDates.size();
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
