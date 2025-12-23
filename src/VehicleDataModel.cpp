#include "VehicleDataModel.h"
#include <QGeoCoordinate>
#include <QThread>
#include <QTimer>
#include <algorithm>

VehicleDataModel::VehicleDataModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_dataProcessingTimer(new QTimer(this))
{
    // Set up timer for batch processing
    m_dataProcessingTimer->setSingleShot(true);
    m_dataProcessingTimer->setInterval(100); // 100ms delay for batch processing
    connect(m_dataProcessingTimer, &QTimer::timeout, this, &VehicleDataModel::processPendingData);
    
    // Reserve memory for better performance
    m_vehicleRecords.reserve(10000); // Reserve space for 10k records
    m_timeIndex.reserve(1000); // Reserve space for time index
}

int VehicleDataModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_vehicleRecords.size();
}

QVariant VehicleDataModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_vehicleRecords.size()) {
        return QVariant();
    }
    
    const ExcelDataReader::VehicleRecord& record = m_vehicleRecords.at(index.row());
    
    switch (role) {
    case PlateNumberRole:
        return record.plateNumber;
    case PositionRole:
        return QVariant::fromValue(record.coordinate());
    case SpeedRole:
        return record.speed;
    case DirectionRole:
        return record.direction;
    case TimestampRole:
        return record.timestamp;
    case ColorRole:
        return record.vehicleColor;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> VehicleDataModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[PlateNumberRole] = "plateNumber";
    roles[PositionRole] = "position";
    roles[SpeedRole] = "speed";
    roles[DirectionRole] = "direction";
    roles[TimestampRole] = "timestamp";
    roles[ColorRole] = "color";
    return roles;
}

void VehicleDataModel::setVehicleData(const QList<ExcelDataReader::VehicleRecord>& records)
{
    // Clear existing data and cache
    beginResetModel();
    m_vehicleRecords.clear();
    m_timeIndex.clear();
    clearCache();
    
    if (records.size() > m_batchSize) {
        // Process large datasets in batches
        m_pendingRecords = records;
        m_dataProcessingTimer->start();
        
        // Set initial time range from first few records
        if (!records.isEmpty()) {
            m_startTime = records.first().timestamp;
            m_endTime = records.last().timestamp;
        }
    } else {
        // Process small datasets immediately
        m_vehicleRecords = records;
        calculateTimeRange();
        if (m_timeIndexingEnabled) {
            buildTimeIndex();
        }
    }
    
    endResetModel();
    emit dataChanged();
}

void VehicleDataModel::processPendingData()
{
    if (m_pendingRecords.isEmpty()) {
        return;
    }
    
    int startIndex = m_vehicleRecords.size();
    int endIndex = qMin(startIndex + m_batchSize, m_pendingRecords.size());
    
    // Process batch
    beginInsertRows(QModelIndex(), startIndex, startIndex + (endIndex - startIndex) - 1);
    
    for (int i = startIndex; i < endIndex; ++i) {
        const auto& record = m_pendingRecords[i];
        m_vehicleRecords.append(record);
        
        if (m_timeIndexingEnabled) {
            addToTimeIndex(record, m_vehicleRecords.size() - 1);
        }
    }
    
    endInsertRows();
    
    // Update progress
    int progress = (endIndex * 100) / m_pendingRecords.size();
    emit dataProcessingProgress(progress);
    
    // Continue processing if more data remains
    if (endIndex < m_pendingRecords.size()) {
        m_dataProcessingTimer->start();
    } else {
        // Finished processing
        m_pendingRecords.clear();
        calculateTimeRange();
        emit dataProcessingProgress(100);
    }
}

QList<VehicleDataModel::VehicleState> VehicleDataModel::getVehicleStatesAtTime(const QDateTime& time)
{
    if (!time.isValid()) {
        return QList<VehicleState>();
    }
    
    qint64 timeKey = timeToKey(time);
    
    // Check cache first
    auto it = m_stateCache.find(timeKey);
    if (it != m_stateCache.end()) {
        return it.value();
    }
    
    // Compute states and cache result
    QList<VehicleState> states = computeVehicleStatesAtTime(time);
    
    // Limit cache size to prevent memory issues
    if (m_stateCache.size() > 1000) {
        m_stateCache.clear();
    }
    m_stateCache[timeKey] = states;
    
    return states;
}

QDateTime VehicleDataModel::getStartTime() const
{
    return m_startTime;
}

QDateTime VehicleDataModel::getEndTime() const
{
    return m_endTime;
}

QStringList VehicleDataModel::getVehicleList() const
{
    QStringList vehicles;
    QSet<QString> uniqueVehicles; // Use QSet for O(1) lookups
    
    for (const auto& record : m_vehicleRecords) {
        if (!uniqueVehicles.contains(record.plateNumber)) {
            uniqueVehicles.insert(record.plateNumber);
            vehicles.append(record.plateNumber);
        }
    }
    return vehicles;
}

// Private helper methods

void VehicleDataModel::calculateTimeRange()
{
    if (m_vehicleRecords.isEmpty()) {
        m_startTime = QDateTime();
        m_endTime = QDateTime();
        return;
    }
    
    m_startTime = m_vehicleRecords.first().timestamp;
    m_endTime = m_vehicleRecords.last().timestamp;
    
    // Use std::minmax_element for better performance on large datasets
    auto minMaxPair = std::minmax_element(m_vehicleRecords.begin(), m_vehicleRecords.end(),
        [](const ExcelDataReader::VehicleRecord& a, const ExcelDataReader::VehicleRecord& b) {
            return a.timestamp < b.timestamp;
        });
    
    if (minMaxPair.first != m_vehicleRecords.end()) {
        m_startTime = minMaxPair.first->timestamp;
    }
    if (minMaxPair.second != m_vehicleRecords.end()) {
        m_endTime = minMaxPair.second->timestamp;
    }
    
    // Log time range information
    if (m_startTime.isValid() && m_endTime.isValid()) {
        qint64 totalDays = m_startTime.daysTo(m_endTime);
        qint64 totalHours = m_startTime.secsTo(m_endTime) / 3600;
    }
}

void VehicleDataModel::buildTimeIndex()
{
    m_timeIndex.clear();
    m_timeIndex.reserve(m_vehicleRecords.size() / 10); // Estimate index size
    
    for (int i = 0; i < m_vehicleRecords.size(); ++i) {
        addToTimeIndex(m_vehicleRecords[i], i);
    }
    
}

void VehicleDataModel::addToTimeIndex(const ExcelDataReader::VehicleRecord& record, int index)
{
    qint64 timeKey = timeToKey(record.timestamp);
    m_timeIndex[timeKey].append(index);
}

QList<VehicleDataModel::VehicleState> VehicleDataModel::computeVehicleStatesAtTime(const QDateTime& time)
{
    QList<VehicleState> states;
    
    if (m_timeIndexingEnabled && !m_timeIndex.isEmpty()) {
        // Use time index for fast lookup - optimized for long-term data
        qint64 timeKey = timeToKey(time);
        
        // Try to find exact match first
        auto it = m_timeIndex.find(timeKey);
        
        // If no exact match, use binary search approach for better performance on large datasets
        if (it == m_timeIndex.end()) {
            // For year-long data, use a more efficient search strategy
            qint64 searchRangeMinutes = 30; // Start with 30 minutes for year-long data
            
            // Adjust search range based on data density
            if (!m_vehicleRecords.isEmpty()) {
                qint64 totalTimeSpan = m_startTime.msecsTo(m_endTime);
                if (totalTimeSpan > 86400000) { // More than 1 day
                    // For very long ranges (months/years), increase search window
                    qint64 totalDays = totalTimeSpan / 86400000;
                    if (totalDays > 30) { // More than 1 month
                        searchRangeMinutes = qMin(240LL, totalDays / 10); // Up to 4 hours for very sparse data
                    } else if (totalDays > 7) { // More than 1 week
                        searchRangeMinutes = 120; // 2 hours for weekly data
                    } else {
                        searchRangeMinutes = 60; // 1 hour for daily data
                    }
                }
            }
            
            // Use efficient range search instead of full iteration
            qint64 minKey = timeKey - searchRangeMinutes;
            qint64 maxKey = timeKey + searchRangeMinutes;
            
            // Find the best match within range using QHash iteration
            qint64 bestKey = -1;
            qint64 minDiff = LLONG_MAX;
            
            for (auto keyIt = m_timeIndex.begin(); keyIt != m_timeIndex.end(); ++keyIt) {
                qint64 currentKey = keyIt.key();
                if (currentKey >= minKey && currentKey <= maxKey) {
                    qint64 diff = qAbs(currentKey - timeKey);
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
                if (index < m_vehicleRecords.size()) {
                    const auto& record = m_vehicleRecords[index];
                    VehicleState state;
                    state.plateNumber = record.plateNumber;
                    state.position = record.coordinate();
                    state.speed = record.speed;
                    state.direction = record.direction;
                    state.timestamp = record.timestamp;
                    state.color = record.vehicleColor;
                    states.append(state);
                }
            }
        }
    } else {
        // Optimized linear search for long-term data without index
        qint64 searchWindowMs = 1800000; // 30 minutes default for year-long data
        
        // Adaptive search window based on data density and time span
        if (!m_vehicleRecords.isEmpty()) {
            qint64 totalTimeSpan = m_startTime.msecsTo(m_endTime);
            qint64 recordDensity = m_vehicleRecords.size() > 0 ? totalTimeSpan / m_vehicleRecords.size() : 3600000;
            
            if (totalTimeSpan > 31536000000LL) { // More than 1 year
                searchWindowMs = qMax(3600000LL, recordDensity * 3); // At least 1 hour, or 3x record density
            } else if (totalTimeSpan > 2592000000LL) { // More than 1 month
                searchWindowMs = qMax(1800000LL, recordDensity * 2); // At least 30 minutes, or 2x record density
            } else if (totalTimeSpan > 604800000LL) { // More than 1 week
                searchWindowMs = qMax(900000LL, recordDensity * 2); // At least 15 minutes, or 2x record density
            } else if (totalTimeSpan > 86400000) { // More than 1 day
                searchWindowMs = qMax(300000LL, recordDensity * 2); // At least 5 minutes, or 2x record density
            }
        }
        
        // Use binary search to find approximate position for better performance
        if (m_vehicleRecords.size() > 1000) {
            // For large datasets, use binary search to narrow down the search range
            auto compareTime = [](const ExcelDataReader::VehicleRecord& record, const QDateTime& targetTime) {
                return record.timestamp < targetTime;
            };
            
            auto lowerBound = std::lower_bound(m_vehicleRecords.begin(), m_vehicleRecords.end(), time, compareTime);
            
            // Search around the found position
            int startIdx = qMax(0, static_cast<int>(lowerBound - m_vehicleRecords.begin()) - 500);
            int endIdx = qMin(m_vehicleRecords.size(), startIdx + 1000);
            
            for (int i = startIdx; i < endIdx; ++i) {
                const auto& record = m_vehicleRecords[i];
                if (qAbs(record.timestamp.msecsTo(time)) < searchWindowMs) {
                    VehicleState state;
                    state.plateNumber = record.plateNumber;
                    state.position = record.coordinate();
                    state.speed = record.speed;
                    state.direction = record.direction;
                    state.timestamp = record.timestamp;
                    state.color = record.vehicleColor;
                    states.append(state);
                }
            }
        } else {
            // For smaller datasets, use the original linear search
            for (const auto& record : m_vehicleRecords) {
                if (qAbs(record.timestamp.msecsTo(time)) < searchWindowMs) {
                    VehicleState state;
                    state.plateNumber = record.plateNumber;
                    state.position = record.coordinate();
                    state.speed = record.speed;
                    state.direction = record.direction;
                    state.timestamp = record.timestamp;
                    state.color = record.vehicleColor;
                    states.append(state);
                }
            }
        }
    }
    
    return states;
}

qint64 VehicleDataModel::timeToKey(const QDateTime& time) const
{
    // Round to nearest minute for indexing
    return time.toMSecsSinceEpoch() / 60000; // Minutes since epoch
}
