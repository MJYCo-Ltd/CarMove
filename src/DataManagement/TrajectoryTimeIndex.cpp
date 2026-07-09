#include "DataManagement/TrajectoryTimeIndex.h"

#include <algorithm>
#include <climits>

TrajectoryTimeIndex::TrajectoryTimeIndex(QObject* parent)
    : QObject(parent)
{
    m_vehicleRecords.reserve(10000);
    m_timeIndex.reserve(1000);
}

void TrajectoryTimeIndex::setTrajectoryData(const QList<TrajectoryPoint>& records)
{
    m_vehicleRecords = records;
    m_timeIndex.clear();
    clearCache();
    calculateTimeRange();
    buildTimeIndex();
}

QList<TrajectoryTimeIndex::VehicleSnapshot> TrajectoryTimeIndex::vehicleSnapshotsAtTime(const QDateTime& time)
{
    if (!time.isValid()) {
        return {};
    }

    const qint64 timeKey = timeToKey(time);
    const auto cached = m_stateCache.constFind(timeKey);
    if (cached != m_stateCache.constEnd()) {
        return cached.value();
    }

    const QList<VehicleSnapshot> states = computeVehicleSnapshotsAtTime(time);
    if (m_stateCache.size() > 1000) {
        m_stateCache.clear();
    }
    m_stateCache.insert(timeKey, states);
    return states;
}

void TrajectoryTimeIndex::calculateTimeRange()
{
    if (m_vehicleRecords.isEmpty()) {
        m_startTime = QDateTime();
        m_endTime = QDateTime();
        return;
    }

    const auto minMaxPair = std::minmax_element(m_vehicleRecords.begin(),
                                                m_vehicleRecords.end(),
                                                [](const TrajectoryPoint& a, const TrajectoryPoint& b) {
                                                    return a.timestamp < b.timestamp;
                                                });
    m_startTime = minMaxPair.first->timestamp;
    m_endTime = minMaxPair.second->timestamp;
}

void TrajectoryTimeIndex::buildTimeIndex()
{
    m_timeIndex.clear();
    m_timeIndex.reserve(m_vehicleRecords.size() / 10);
    for (int i = 0; i < m_vehicleRecords.size(); ++i) {
        addToTimeIndex(m_vehicleRecords.at(i), i);
    }
}

void TrajectoryTimeIndex::addToTimeIndex(const TrajectoryPoint& record, int index)
{
    m_timeIndex[timeToKey(record.timestamp)].append(index);
}

QList<TrajectoryTimeIndex::VehicleSnapshot> TrajectoryTimeIndex::computeVehicleSnapshotsAtTime(
    const QDateTime& time)
{
    QList<VehicleSnapshot> states;
    if (m_vehicleRecords.isEmpty()) {
        return states;
    }

    if (!m_timeIndex.isEmpty()) {
        const qint64 timeKey = timeToKey(time);
        auto it = m_timeIndex.find(timeKey);

        if (it == m_timeIndex.end()) {
            qint64 searchRangeMinutes = 30;
            const qint64 totalTimeSpan = m_startTime.msecsTo(m_endTime);
            if (totalTimeSpan > 86400000) {
                const qint64 totalDays = totalTimeSpan / 86400000;
                if (totalDays > 30) {
                    searchRangeMinutes = qMin(240LL, totalDays / 10);
                } else if (totalDays > 7) {
                    searchRangeMinutes = 120;
                } else {
                    searchRangeMinutes = 60;
                }
            }

            const qint64 minKey = timeKey - searchRangeMinutes;
            const qint64 maxKey = timeKey + searchRangeMinutes;
            qint64 bestKey = -1;
            qint64 minDiff = LLONG_MAX;

            for (auto keyIt = m_timeIndex.constBegin(); keyIt != m_timeIndex.constEnd(); ++keyIt) {
                const qint64 currentKey = keyIt.key();
                if (currentKey >= minKey && currentKey <= maxKey) {
                    const qint64 diff = qAbs(currentKey - timeKey);
                    if (diff < minDiff) {
                        minDiff = diff;
                        bestKey = currentKey;
                    }
                }
            }

            if (bestKey != -1) {
                it = m_timeIndex.find(bestKey);
            }
        }

        if (it != m_timeIndex.end()) {
            for (int index : it.value()) {
                if (index < 0 || index >= m_vehicleRecords.size()) {
                    continue;
                }
                const TrajectoryPoint& record = m_vehicleRecords.at(index);
                states.append({record.plateNumber,
                               record.coordinate(),
                               record.speed,
                               record.direction,
                               record.timestamp,
                               record.vehicleColor});
            }
            return states;
        }
    }

    qint64 searchWindowMs = 1800000;
    const qint64 totalTimeSpan = m_startTime.msecsTo(m_endTime);
    const qint64 recordDensity =
        m_vehicleRecords.isEmpty() ? 3600000 : totalTimeSpan / m_vehicleRecords.size();

    if (totalTimeSpan > 31536000000LL) {
        searchWindowMs = qMax(3600000LL, recordDensity * 3);
    } else if (totalTimeSpan > 2592000000LL) {
        searchWindowMs = qMax(1800000LL, recordDensity * 2);
    } else if (totalTimeSpan > 604800000LL) {
        searchWindowMs = qMax(900000LL, recordDensity * 2);
    } else if (totalTimeSpan > 86400000) {
        searchWindowMs = qMax(300000LL, recordDensity * 2);
    }

    if (m_vehicleRecords.size() > 1000) {
        const auto lowerBound = std::lower_bound(m_vehicleRecords.begin(),
                                                 m_vehicleRecords.end(),
                                                 time,
                                                 [](const TrajectoryPoint& record, const QDateTime& targetTime) {
                                                     return record.timestamp < targetTime;
                                                 });
        const int startIdx = qMax(0, static_cast<int>(lowerBound - m_vehicleRecords.begin()) - 500);
        const int endIdx = qMin(m_vehicleRecords.size(), startIdx + 1000);

        for (int i = startIdx; i < endIdx; ++i) {
            const TrajectoryPoint& record = m_vehicleRecords.at(i);
            if (qAbs(record.timestamp.msecsTo(time)) < searchWindowMs) {
                states.append({record.plateNumber,
                               record.coordinate(),
                               record.speed,
                               record.direction,
                               record.timestamp,
                               record.vehicleColor});
            }
        }
        return states;
    }

    for (const TrajectoryPoint& record : m_vehicleRecords) {
        if (qAbs(record.timestamp.msecsTo(time)) < searchWindowMs) {
            states.append({record.plateNumber,
                           record.coordinate(),
                           record.speed,
                           record.direction,
                           record.timestamp,
                           record.vehicleColor});
        }
    }

    return states;
}

qint64 TrajectoryTimeIndex::timeToKey(const QDateTime& time) const
{
    return time.toMSecsSinceEpoch() / 60000;
}
