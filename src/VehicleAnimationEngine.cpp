#include "VehicleAnimationEngine.h"
#include <QtMath>
#include <algorithm>

VehicleAnimationEngine::VehicleAnimationEngine(QObject *parent)
    : QObject(parent)
    , m_vehicleModel(nullptr)
    , m_animationTimer(new QTimer(this))
    , m_cacheCleanupTimer(new QTimer(this))
    , m_playbackState(Stopped)
    , m_playbackSpeed(1.0)
    , m_currentProgress(0.0)
    , m_isDragging(false)
{
    updateTimerInterval();
    connect(m_animationTimer, &QTimer::timeout, this, &VehicleAnimationEngine::updateAnimation);
    
    // Set up cache cleanup timer
    m_cacheCleanupTimer->setInterval(30000); // Clean cache every 30 seconds
    m_cacheCleanupTimer->setSingleShot(false);
    connect(m_cacheCleanupTimer, &QTimer::timeout, this, &VehicleAnimationEngine::cleanupCache);
    m_cacheCleanupTimer->start();
    
    // Initialize frame timer
    m_frameTimer.start();
}

void VehicleAnimationEngine::setVehicleModel(VehicleDataModel* model)
{
    m_vehicleModel = model;
    if (model) {
        m_startTime = model->getStartTime();
        m_endTime = model->getEndTime();
        m_currentTime = m_startTime;
        
        // Clear cache when model changes
        m_vehicleStateCache.clear();
        m_lastKnownPositions.clear();
        
        // Emit initial time change to update UI
        emit currentTimeChanged(m_currentTime);
        
        // Update initial vehicle positions
        updateVehiclePositions();
    } else {
    }
}

void VehicleAnimationEngine::setPlaybackSpeed(double multiplier)
{
    m_playbackSpeed = multiplier;
    updateTimerInterval(); // Adjust timer interval based on speed
}

void VehicleAnimationEngine::setCurrentTime(const QDateTime& time)
{
    m_currentTime = time;
    
    // Update vehicle positions at the new time (optimized for dragging)
    if (!m_isDragging || m_frameTimer.elapsed() - m_lastFrameTime > 16) { // Limit to ~60fps during dragging
        updateVehiclePositions();
        m_lastFrameTime = m_frameTimer.elapsed();
    }
    
    emit currentTimeChanged(time);
    
    // Calculate progress
    if (m_startTime.isValid() && m_endTime.isValid()) {
        qint64 totalMs = m_startTime.msecsTo(m_endTime);
        qint64 currentMs = m_startTime.msecsTo(time);
        m_currentProgress = totalMs > 0 ? static_cast<double>(currentMs) / totalMs : 0.0;
        emit progressChanged(m_currentProgress);
    }
}

void VehicleAnimationEngine::seekToProgress(double progress)
{
    m_currentProgress = qBound(0.0, progress, 1.0);
    
    if (m_startTime.isValid() && m_endTime.isValid()) {
        qint64 totalMs = m_startTime.msecsTo(m_endTime);
        qint64 targetMs = static_cast<qint64>(totalMs * progress);
        QDateTime targetTime = m_startTime.addMSecs(targetMs);
        m_currentTime = targetTime;
        
        // Update vehicle positions at the new time immediately
        updateVehiclePositions();
        
        emit currentTimeChanged(targetTime);
        emit progressChanged(m_currentProgress);
    }
}

void VehicleAnimationEngine::play()
{
    if (m_playbackState != Playing) {
        m_playbackState = Playing;
        m_animationTimer->start();
        emit playbackStateChanged(Playing);
    }
}

void VehicleAnimationEngine::pause()
{
    if (m_playbackState == Playing) {
        m_playbackState = Paused;
        m_animationTimer->stop();
        emit playbackStateChanged(Paused);
    }
}

void VehicleAnimationEngine::stop()
{
    m_playbackState = Stopped;
    m_animationTimer->stop();
    m_currentProgress = 0.0;
    setCurrentTime(m_startTime);
    emit playbackStateChanged(Stopped);
}

void VehicleAnimationEngine::seekToTime(const QDateTime& time)
{
    setCurrentTime(time);
}

void VehicleAnimationEngine::onTimeSliderDragged(double progress)
{
    seekToProgress(progress);
}

void VehicleAnimationEngine::setDraggingMode(bool isDragging)
{
    m_isDragging = isDragging;
}

void VehicleAnimationEngine::updateAnimation()
{
    if (m_playbackState != Playing || !m_vehicleModel) {
        return;
    }
    
    // Update progress based on playback speed - optimized for long-term data
    double timeStep = (m_animationTimer->interval() * m_playbackSpeed) / 1000.0; // seconds
    if (m_startTime.isValid() && m_endTime.isValid()) {
        qint64 totalSeconds = m_startTime.secsTo(m_endTime);
        if (totalSeconds > 0) {
            double progressStep = timeStep / totalSeconds;
            
            // Adaptive step size for different time ranges
            if (totalSeconds > 31536000) { // More than 1 year
                // For year-long data, use logarithmic scaling to make animation reasonable
                // Complete animation in ~5 minutes at 1x speed
                double targetAnimationSeconds = 300.0; // 5 minutes
                double scaleFactor = targetAnimationSeconds / totalSeconds;
                progressStep = (timeStep / targetAnimationSeconds);
            } else if (totalSeconds > 2592000) { // More than 1 month
                // For monthly data, complete in ~3 minutes at 1x speed
                double targetAnimationSeconds = 180.0;
                progressStep = (timeStep / targetAnimationSeconds);
            } else if (totalSeconds > 604800) { // More than 1 week
                // For weekly data, complete in ~2 minutes at 1x speed
                double targetAnimationSeconds = 120.0;
                progressStep = (timeStep / targetAnimationSeconds);
            } else if (totalSeconds > 86400) { // More than 1 day
                // For daily data, complete in ~1 minute at 1x speed
                double targetAnimationSeconds = 60.0;
                progressStep = (timeStep / targetAnimationSeconds);
            }
            // For data less than 1 day, use real-time or near real-time playback
            
            m_currentProgress += progressStep;
            
            if (m_currentProgress >= 1.0) {
                m_currentProgress = 1.0;
                stop(); // Auto-stop at end
                return;
            }
            
            // Calculate current time based on progress
            qint64 totalMs = m_startTime.msecsTo(m_endTime);
            qint64 currentMs = static_cast<qint64>(totalMs * m_currentProgress);
            m_currentTime = m_startTime.addMSecs(currentMs);
            
            // Update vehicle positions at current time
            updateVehiclePositions();
            
            emit currentTimeChanged(m_currentTime);
            emit progressChanged(m_currentProgress);
        }
    }
}

QGeoCoordinate VehicleAnimationEngine::interpolatePosition(const QGeoCoordinate& start,
                                                         const QGeoCoordinate& end,
                                                         double ratio) const
{
    // Simple linear interpolation - will be enhanced in later tasks
    double lat = start.latitude() + (end.latitude() - start.latitude()) * ratio;
    double lng = start.longitude() + (end.longitude() - start.longitude()) * ratio;
    return QGeoCoordinate(lat, lng);
}

int VehicleAnimationEngine::interpolateDirection(int startDir, int endDir, double ratio) const
{
    // Handle circular interpolation for direction (0-360 degrees)
    int diff = endDir - startDir;
    if (diff > 180) {
        diff -= 360;
    } else if (diff < -180) {
        diff += 360;
    }
    
    int result = startDir + static_cast<int>(diff * ratio);
    if (result < 0) result += 360;
    if (result >= 360) result -= 360;
    
    return result;
}

VehicleDataModel::VehicleState VehicleAnimationEngine::interpolateVehicleState(double progress) const
{
    if (!m_vehicleModel || m_vehicleModel->rowCount() == 0) {
        return VehicleDataModel::VehicleState();
    }
    
    // Find the records that bracket the current time
    QDateTime targetTime = m_startTime.addMSecs(static_cast<qint64>(
        m_startTime.msecsTo(m_endTime) * progress));
    
    // For now, find the closest record to the target time
    // This is a simplified implementation - in a full implementation,
    // we would interpolate between adjacent records
    ExcelDataReader::VehicleRecord closestRecord;
    qint64 minTimeDiff = LLONG_MAX;
    bool foundRecord = false;
    
    for (int i = 0; i < m_vehicleModel->rowCount(); ++i) {
        QModelIndex index = m_vehicleModel->index(i);
        QDateTime recordTime = m_vehicleModel->data(index, Qt::UserRole + 5).toDateTime(); // TimestampRole
        qint64 timeDiff = qAbs(targetTime.msecsTo(recordTime));
        
        if (timeDiff < minTimeDiff) {
            minTimeDiff = timeDiff;
            closestRecord.plateNumber = m_vehicleModel->data(index, Qt::UserRole + 1).toString();
            closestRecord.speed = m_vehicleModel->data(index, Qt::UserRole + 3).toDouble();
            closestRecord.direction = m_vehicleModel->data(index, Qt::UserRole + 4).toInt();
            closestRecord.timestamp = recordTime;
            closestRecord.vehicleColor = m_vehicleModel->data(index, Qt::UserRole + 6).toString();
            
            // Get coordinate from position role
            QVariant posVar = m_vehicleModel->data(index, Qt::UserRole + 2);
            if (posVar.canConvert<QGeoCoordinate>()) {
                QGeoCoordinate coord = posVar.value<QGeoCoordinate>();
                closestRecord.longitude = coord.longitude();
                closestRecord.latitude = coord.latitude();
            }
            
            foundRecord = true;
        }
    }
    
    if (!foundRecord) {
        return VehicleDataModel::VehicleState();
    }
    
    // Convert to VehicleState
    VehicleDataModel::VehicleState state;
    state.plateNumber = closestRecord.plateNumber;
    state.position = QGeoCoordinate(closestRecord.latitude, closestRecord.longitude);
    state.speed = closestRecord.speed;
    state.direction = closestRecord.direction;
    state.timestamp = closestRecord.timestamp;
    state.color = closestRecord.vehicleColor;
    
    return state;
}

void VehicleAnimationEngine::updateVehiclePositions()
{
    if (!m_vehicleModel) {
        return;
    }
    
    if (m_vehicleModel->rowCount() == 0) {
        return;
    }
    
    // Get all unique vehicles and update their positions - optimized for long-term data
    QStringList vehicles = m_vehicleModel->getVehicleList();
    
    for (const QString& plateNumber : vehicles) {
        // Use cached vehicle records if available for better performance
        QString cacheKey = QString("%1_%2").arg(plateNumber).arg(m_currentTime.toMSecsSinceEpoch() / 60000); // Cache per minute
        
        // Check cache first
        bool foundInCache = false;
        VehicleDataModel::VehicleState cachedState;
        auto cachedStateIt = m_vehicleStateCache.find(cacheKey);
        if (cachedStateIt != m_vehicleStateCache.end()) {
            cachedState = cachedStateIt.value();
            foundInCache = true;
        }
        
        // Emit signal if found in cache
        if (foundInCache) {
            emit vehiclePositionUpdated(plateNumber, cachedState.position, cachedState.direction, cachedState.speed);
            continue;
        }
        
        // Find the vehicle records that bracket the current time
        QList<ExcelDataReader::VehicleRecord> vehicleRecords;
        
        // Optimized record collection for large datasets
        if (m_vehicleModel->rowCount() > 10000) {
            // For very large datasets, use a more efficient approach
            // First, estimate the time range we need to search
            qint64 searchRangeMs = 7200000; // 2 hours default for year-long data
            
            // Adjust search range based on data span
            QDateTime modelStart = m_vehicleModel->getStartTime();
            QDateTime modelEnd = m_vehicleModel->getEndTime();
            if (modelStart.isValid() && modelEnd.isValid()) {
                qint64 totalSpan = modelStart.msecsTo(modelEnd);
                if (totalSpan > 31536000000LL) { // More than 1 year
                    searchRangeMs = 14400000; // 4 hours for year-long data
                } else if (totalSpan > 2592000000LL) { // More than 1 month
                    searchRangeMs = 7200000; // 2 hours for monthly data
                } else if (totalSpan > 604800000LL) { // More than 1 week
                    searchRangeMs = 3600000; // 1 hour for weekly data
                }
            }
            
            QDateTime searchStart = m_currentTime.addMSecs(-searchRangeMs);
            QDateTime searchEnd = m_currentTime.addMSecs(searchRangeMs);
            
            // Use a more targeted search approach
            for (int i = 0; i < m_vehicleModel->rowCount(); i += 10) { // Sample every 10th record for efficiency
                QModelIndex index = m_vehicleModel->index(i);
                QString recordPlate = m_vehicleModel->data(index, Qt::UserRole + 1).toString();
                QDateTime recordTime = m_vehicleModel->data(index, Qt::UserRole + 5).toDateTime();
                
                if (recordPlate == plateNumber && recordTime >= searchStart && recordTime <= searchEnd) {
                    ExcelDataReader::VehicleRecord record;
                    record.plateNumber = recordPlate;
                    record.speed = m_vehicleModel->data(index, Qt::UserRole + 3).toDouble();
                    record.direction = m_vehicleModel->data(index, Qt::UserRole + 4).toInt();
                    record.timestamp = recordTime;
                    record.vehicleColor = m_vehicleModel->data(index, Qt::UserRole + 6).toString();
                    
                    QVariant posVar = m_vehicleModel->data(index, Qt::UserRole + 2);
                    if (posVar.canConvert<QGeoCoordinate>()) {
                        QGeoCoordinate coord = posVar.value<QGeoCoordinate>();
                        record.longitude = coord.longitude();
                        record.latitude = coord.latitude();
                    }
                    
                    vehicleRecords.append(record);
                }
                
                // Limit the number of records to prevent performance issues
                if (vehicleRecords.size() > 100) {
                    break;
                }
            }
        } else {
            // For smaller datasets, use the original approach
            for (int i = 0; i < m_vehicleModel->rowCount(); ++i) {
                QModelIndex index = m_vehicleModel->index(i);
                QString recordPlate = m_vehicleModel->data(index, Qt::UserRole + 1).toString();
                
                if (recordPlate == plateNumber) {
                    ExcelDataReader::VehicleRecord record;
                    record.plateNumber = recordPlate;
                    record.speed = m_vehicleModel->data(index, Qt::UserRole + 3).toDouble();
                    record.direction = m_vehicleModel->data(index, Qt::UserRole + 4).toInt();
                    record.timestamp = m_vehicleModel->data(index, Qt::UserRole + 5).toDateTime();
                    record.vehicleColor = m_vehicleModel->data(index, Qt::UserRole + 6).toString();
                    
                    QVariant posVar = m_vehicleModel->data(index, Qt::UserRole + 2);
                    if (posVar.canConvert<QGeoCoordinate>()) {
                        QGeoCoordinate coord = posVar.value<QGeoCoordinate>();
                        record.longitude = coord.longitude();
                        record.latitude = coord.latitude();
                    }
                    
                    vehicleRecords.append(record);
                }
            }
        }
        
        if (vehicleRecords.isEmpty()) {
            continue;
        }
        
        // Sort records by timestamp
        std::sort(vehicleRecords.begin(), vehicleRecords.end(), 
                 [](const ExcelDataReader::VehicleRecord& a, const ExcelDataReader::VehicleRecord& b) {
                     return a.timestamp < b.timestamp;
                 });
        
        // Find the appropriate record or interpolate between records
        ExcelDataReader::VehicleRecord currentRecord;
        bool foundRecord = false;
        
        // Find records that bracket the current time
        for (int i = 0; i < vehicleRecords.size() - 1; ++i) {
            if (vehicleRecords[i].timestamp <= m_currentTime && 
                m_currentTime <= vehicleRecords[i + 1].timestamp) {
                
                // Interpolate between records[i] and records[i+1]
                qint64 totalMs = vehicleRecords[i].timestamp.msecsTo(vehicleRecords[i + 1].timestamp);
                qint64 currentMs = vehicleRecords[i].timestamp.msecsTo(m_currentTime);
                
                double ratio = totalMs > 0 ? static_cast<double>(currentMs) / totalMs : 0.0;
                
                // Interpolate position
                QGeoCoordinate startPos(vehicleRecords[i].latitude, vehicleRecords[i].longitude);
                QGeoCoordinate endPos(vehicleRecords[i + 1].latitude, vehicleRecords[i + 1].longitude);
                QGeoCoordinate interpolatedPos = interpolatePosition(startPos, endPos, ratio);
                
                // Interpolate direction
                int interpolatedDir = interpolateDirection(vehicleRecords[i].direction, 
                                                         vehicleRecords[i + 1].direction, ratio);
                
                // Interpolate speed
                double interpolatedSpeed = vehicleRecords[i].speed + 
                                         (vehicleRecords[i + 1].speed - vehicleRecords[i].speed) * ratio;
                
                currentRecord.plateNumber = plateNumber;
                currentRecord.latitude = interpolatedPos.latitude();
                currentRecord.longitude = interpolatedPos.longitude();
                currentRecord.direction = interpolatedDir;
                currentRecord.speed = interpolatedSpeed;
                currentRecord.timestamp = m_currentTime;
                currentRecord.vehicleColor = vehicleRecords[i].vehicleColor;
                
                foundRecord = true;
                break;
            }
        }
        
        // If no interpolation possible, use closest record within reasonable time range
        if (!foundRecord) {
            qint64 minTimeDiff = LLONG_MAX;
            qint64 maxSearchRange = 14400000; // 4 hours default for year-long data
            
            // Adjust search range based on data density and time span
            if (vehicleRecords.size() > 1) {
                qint64 totalTimeSpan = vehicleRecords.first().timestamp.msecsTo(vehicleRecords.last().timestamp);
                qint64 avgGap = totalTimeSpan / (vehicleRecords.size() - 1);
                
                // For very sparse data (year-long), allow larger gaps
                if (totalTimeSpan > 31536000000LL) { // More than 1 year
                    maxSearchRange = qMax(14400000LL, avgGap * 3); // At least 4 hours, or 3x average gap
                } else if (totalTimeSpan > 2592000000LL) { // More than 1 month
                    maxSearchRange = qMax(7200000LL, avgGap * 2); // At least 2 hours, or 2x average gap
                } else if (totalTimeSpan > 604800000LL) { // More than 1 week
                    maxSearchRange = qMax(3600000LL, avgGap * 2); // At least 1 hour, or 2x average gap
                } else {
                    maxSearchRange = qMax(1800000LL, avgGap * 2); // At least 30 minutes, or 2x average gap
                }
            }
            
            for (const auto& record : vehicleRecords) {
                qint64 timeDiff = qAbs(m_currentTime.msecsTo(record.timestamp));
                if (timeDiff < minTimeDiff && timeDiff <= maxSearchRange) {
                    minTimeDiff = timeDiff;
                    currentRecord = record;
                    foundRecord = true;
                }
            }
            
            if (foundRecord) {
            }
        }
        
        if (foundRecord) {
            QGeoCoordinate position(currentRecord.latitude, currentRecord.longitude);
            
            // Cache the result for better performance
            VehicleDataModel::VehicleState state;
            state.plateNumber = currentRecord.plateNumber;
            state.position = position;
            state.speed = currentRecord.speed;
            state.direction = currentRecord.direction;
            state.timestamp = currentRecord.timestamp;
            state.color = currentRecord.vehicleColor;
            
            if (m_vehicleStateCache.size() < m_maxCacheSize) {
                m_vehicleStateCache[cacheKey] = state;
            }
            
            // Emit signal
            emit vehiclePositionUpdated(plateNumber, position, 
                                      currentRecord.direction, currentRecord.speed);
        } else {
        }
    }
}

// Performance optimization methods

void VehicleAnimationEngine::updateTimerInterval()
{
    if (m_animationTimer) {
        int interval = qMax(1, static_cast<int>(1000.0 / m_targetFps / m_playbackSpeed));
        m_animationTimer->setInterval(interval);
    }
}

bool VehicleAnimationEngine::shouldUpdatePosition(const QString& plateNumber, const QGeoCoordinate& newPos)
{
    auto it = m_lastKnownPositions.find(plateNumber);
    bool shouldUpdate = true;
    
    if (it != m_lastKnownPositions.end()) {
        const QGeoCoordinate& lastPos = it.value();
        double distance = lastPos.distanceTo(newPos);
        
        // Only update if position changed significantly (reduces unnecessary updates)
        shouldUpdate = distance > MIN_POSITION_CHANGE * 111000; // Convert degrees to meters approximately
    }
    
    return shouldUpdate;
}

void VehicleAnimationEngine::cacheVehicleState(const QString& plateNumber, const VehicleDataModel::VehicleState& state)
{
    // Limit cache size to prevent memory issues
    if (m_vehicleStateCache.size() >= m_maxCacheSize) {
        // Remove oldest entries (simple FIFO)
        auto it = m_vehicleStateCache.begin();
        for (int i = 0; i < m_maxCacheSize / 4 && it != m_vehicleStateCache.end(); ++i) {
            it = m_vehicleStateCache.erase(it);
        }
    }
    
    m_vehicleStateCache[plateNumber] = state;
    m_lastKnownPositions[plateNumber] = state.position;
}

VehicleDataModel::VehicleState VehicleAnimationEngine::getCachedVehicleState(const QString& plateNumber)
{
    auto it = m_vehicleStateCache.find(plateNumber);
    VehicleDataModel::VehicleState result;
    if (it != m_vehicleStateCache.end()) {
        result = it.value();
    }
    
    return result;
}

void VehicleAnimationEngine::cleanupCache()
{
    // Clear cache if it gets too large
    if (m_vehicleStateCache.size() > m_maxCacheSize * 2) {
        m_vehicleStateCache.clear();
        m_lastKnownPositions.clear();
    }
}