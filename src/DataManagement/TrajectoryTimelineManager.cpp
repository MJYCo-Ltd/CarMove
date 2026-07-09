#include "DataManagement/TrajectoryTimelineManager.h"

#include "Core/AppLogger.h"
#include "DataManagement/TrajectorySegmentBreak.h"

#include <QMetaType>
#include <QtGlobal>

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

TrajectoryTimelineManager::TrajectoryTimelineManager(QObject* parent)
    : QObject(parent)
{
}

void TrajectoryTimelineManager::setSelectedVehicle(const QString& plateNumber)
{
    m_selectedVehicle = plateNumber;
}

void TrajectoryTimelineManager::applyTrajectory(const QList<TrajectoryPoint>& trajectory)
{
    m_index.setTrajectoryData(trajectory);
    m_index.clearCache();
    syncTimeRangeFromIndex();
    rebuildSegments(trajectory);
}

void TrajectoryTimelineManager::clearTimeline()
{
    m_index.setTrajectoryData({});
    m_index.clearCache();
    m_startTime = QDateTime();
    m_endTime = QDateTime();
    m_currentTime = QDateTime();
    m_segmentMeta.clear();
    m_displaySegments.clear();
    m_segmentsNeedRebuild = true;
    emit timeRangeChanged();
    emit currentTimeChanged();
    emit segmentsChanged();
}

bool TrajectoryTimelineManager::spansMultipleDays() const
{
    if (!m_startTime.isValid() || !m_endTime.isValid()) {
        return false;
    }
    return m_startTime.date() != m_endTime.date();
}

void TrajectoryTimelineManager::resetTimeline()
{
    if (m_startTime.isValid()) {
        seekToTime(m_startTime);
        return;
    }
    m_currentTime = QDateTime();
    emit currentTimeChanged();
}

void TrajectoryTimelineManager::seekToTime(const QDateTime& time)
{
    if (!time.isValid()) {
        return;
    }

    m_currentTime = time;
    publishVehiclePositionAtCurrentTime();
    emit currentTimeChanged();
}

void TrajectoryTimelineManager::seekToProgress(double progress)
{
    if (!m_startTime.isValid() || !m_endTime.isValid()) {
        return;
    }

    progress = qBound(0.0, progress, 1.0);
    const qint64 totalMs = m_startTime.msecsTo(m_endTime);
    seekToTime(m_startTime.addMSecs(static_cast<qint64>(totalMs * progress)));
}

QDateTime TrajectoryTimelineManager::segmentStartTime(int segmentIndex) const
{
    if (segmentIndex < 0 || segmentIndex >= m_segmentMeta.size()) {
        return {};
    }
    return m_segmentMeta.at(segmentIndex).startTime;
}

QDateTime TrajectoryTimelineManager::segmentEndTime(int segmentIndex) const
{
    if (segmentIndex < 0 || segmentIndex >= m_segmentMeta.size()) {
        return {};
    }
    return m_segmentMeta.at(segmentIndex).endTime;
}

int TrajectoryTimelineManager::activeSegmentIndex() const
{
    if (m_segmentMeta.isEmpty() || !m_currentTime.isValid()) {
        return -1;
    }

    const QDateTime current = m_currentTime;
    for (int i = 0; i < m_segmentMeta.size(); ++i) {
        const SegmentMeta& meta = m_segmentMeta.at(i);
        if (current >= meta.startTime && current <= meta.endTime) {
            return i;
        }
    }

    for (int i = m_segmentMeta.size() - 1; i >= 0; --i) {
        if (current >= m_segmentMeta.at(i).startTime) {
            return i;
        }
    }

    return 0;
}

double TrajectoryTimelineManager::segmentLocalProgress(int segmentIndex) const
{
    if (segmentIndex < 0 || segmentIndex >= m_segmentMeta.size() || !m_currentTime.isValid()) {
        return 0.0;
    }

    const SegmentMeta& meta = m_segmentMeta.at(segmentIndex);
    const qint64 totalMs = meta.startTime.msecsTo(meta.endTime);
    if (totalMs <= 0) {
        return 0.0;
    }

    const qint64 elapsedMs = meta.startTime.msecsTo(m_currentTime);
    return qBound(0.0, static_cast<double>(elapsedMs) / static_cast<double>(totalMs), 1.0);
}

void TrajectoryTimelineManager::seekSegment(int segmentIndex, double localProgress)
{
    if (segmentIndex < 0 || segmentIndex >= m_segmentMeta.size()) {
        return;
    }

    const SegmentMeta& meta = m_segmentMeta.at(segmentIndex);
    if (!meta.startTime.isValid() || !meta.endTime.isValid()) {
        return;
    }

    localProgress = qBound(0.0, localProgress, 1.0);
    const qint64 totalMs = meta.startTime.msecsTo(meta.endTime);
    seekToTime(meta.startTime.addMSecs(static_cast<qint64>(totalMs * localProgress)));
}

int TrajectoryTimelineManager::displaySegmentCount()
{
    return m_displaySegments.size();
}

QVariantList TrajectoryTimelineManager::displaySegmentPath(int segmentIndex) const
{
    if (segmentIndex < 0 || segmentIndex >= m_displaySegments.size()) {
        return {};
    }
    return nestedSegmentToList(m_displaySegments.at(segmentIndex));
}

void TrajectoryTimelineManager::invalidateSegments()
{
    m_segmentMeta.clear();
    m_displaySegments.clear();
    m_segmentsNeedRebuild = true;
    emit segmentsChanged();
}

void TrajectoryTimelineManager::syncTimeRangeFromIndex()
{
    const QDateTime start = m_index.startTime();
    const QDateTime end = m_index.endTime();
    if (m_startTime == start && m_endTime == end) {
        return;
    }

    m_startTime = start;
    m_endTime = end;
    emit timeRangeChanged();
}

void TrajectoryTimelineManager::publishVehiclePositionAtCurrentTime()
{
    if (m_selectedVehicle.isEmpty() || !m_currentTime.isValid()) {
        return;
    }

    const QList<TrajectoryTimeIndex::VehicleSnapshot> states =
        m_index.vehicleSnapshotsAtTime(m_currentTime);
    for (const TrajectoryTimeIndex::VehicleSnapshot& state : states) {
        if (state.plateNumber == m_selectedVehicle) {
            emit vehiclePositionUpdated(state.plateNumber,
                                        state.position,
                                        state.direction,
                                        state.speed);
            return;
        }
    }

    if (states.size() == 1) {
        const TrajectoryTimeIndex::VehicleSnapshot& state = states.first();
        emit vehiclePositionUpdated(state.plateNumber, state.position, state.direction, state.speed);
    }
}

void TrajectoryTimelineManager::rebuildSegments(const QList<TrajectoryPoint>& trajectoryHint)
{
    if (m_isRebuildingSegments) {
        return;
    }
    m_isRebuildingSegments = true;
    m_displaySegments.clear();
    m_segmentMeta.clear();

    const QList<TrajectoryPoint>& trajectory = trajectoryHint;
    if (trajectory.size() < 2) {
        m_segmentsNeedRebuild = false;
        m_isRebuildingSegments = false;
        emit segmentsChanged();
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
            appendNestedSegment(m_displaySegments, currentSegment);
            SegmentMeta meta;
            meta.startTime = segmentStartTime;
            meta.endTime = segmentEndTime;
            m_segmentMeta.append(meta);
        }
        currentSegment = QVariantList();
        segmentStartTime = QDateTime();
        segmentEndTime = QDateTime();
    };

    for (const TrajectoryPoint& record : trajectory) {
        if (!TrajectorySegmentBreak::isDrawableCoordinate(record)) {
            ++invalidPointCount;
            continue;
        }
        ++validPointCount;

        const QGeoCoordinate coordinate(record.latitude, record.longitude);

        const TrajectorySegmentBreak::Evaluation breakEvaluation =
            hasPrevious ? TrajectorySegmentBreak::evaluate(previousCoordinate,
                                                           previousTimestamp,
                                                           coordinate,
                                                           record.timestamp)
                        : TrajectorySegmentBreak::Evaluation{};
        const bool shouldBreak = breakEvaluation.shouldBreak;

        if (shouldBreak) {
            ++breakCount;
            TrajectorySegmentBreak::logGpsJumpAnomaly(record.plateNumber.isEmpty() ? m_selectedVehicle
                                                                                   : record.plateNumber,
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

        currentSegment.append(TrajectorySegmentBreak::coordinateToVariantMap(coordinate));
        previousCoordinate = coordinate;
        previousTimestamp = record.timestamp;
        hasPrevious = true;
    }

    flushSegment();

    if (m_displaySegments.isEmpty() && validPointCount > 0) {
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
    m_isRebuildingSegments = false;
    emit segmentsChanged();
}
