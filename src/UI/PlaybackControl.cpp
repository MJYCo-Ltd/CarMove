#include "UI/PlaybackControl.h"
#include "DataManagement/VehicleDataModel.h"
#include <QtMath>

PlaybackControl::PlaybackControl(VehicleAnimationEngine* engine, VehicleDataModel* model, QObject* parent)
    : QObject(parent)
    , m_engine(engine)
    , m_model(model)
{
    if (m_engine) {
        connect(m_engine, &VehicleAnimationEngine::currentTimeChanged,
                this, &PlaybackControl::onEngineCurrentTimeChanged);
        connect(m_engine, &VehicleAnimationEngine::progressChanged,
                this, &PlaybackControl::onEngineProgressChanged);
        connect(m_engine, QOverload<VehicleAnimationEngine::PlaybackState>::of(&VehicleAnimationEngine::playbackStateChanged),
                this, &PlaybackControl::onEnginePlaybackStateChanged);
    }
}

QDateTime PlaybackControl::startTime() const
{
    return m_model ? m_model->getStartTime() : QDateTime();
}

QDateTime PlaybackControl::endTime() const
{
    return m_model ? m_model->getEndTime() : QDateTime();
}

void PlaybackControl::notifyModelTimeRangeUpdated()
{
    const QDateTime newStart = startTime();
    const QDateTime newEnd = endTime();
    const bool rangeChanged = (newStart != m_lastNotifiedRangeStart || newEnd != m_lastNotifiedRangeEnd);
    m_lastNotifiedRangeStart = newStart;
    m_lastNotifiedRangeEnd = newEnd;
    if (rangeChanged)
        emit timeRangeChanged();
    if (!m_currentTime.isValid() || m_currentTime < newStart || m_currentTime > newEnd) {
        m_currentTime = newStart;
        emit currentTimeChanged();
    }
}

void PlaybackControl::onEngineCurrentTimeChanged(const QDateTime& time)
{
    if (m_currentTime != time) {
        m_currentTime = time;
        emit currentTimeChanged();
    }
}

void PlaybackControl::onEngineProgressChanged(double progress)
{
    if (qAbs(m_playbackProgress - progress) > 0.001) {
        m_playbackProgress = progress;
        emit progressChanged();
    }
}

void PlaybackControl::onEnginePlaybackStateChanged(VehicleAnimationEngine::PlaybackState state)
{
    const bool playing = (state == VehicleAnimationEngine::Playing);
    if (m_isPlaying != playing) {
        m_isPlaying = playing;
        emit playbackStateChanged();
    }
}

void PlaybackControl::startPlayback()
{
    if (m_engine && m_model && m_model->rowCount() > 0)
        m_engine->play();
}

void PlaybackControl::pausePlayback()
{
    if (m_engine)
        m_engine->pause();
}

void PlaybackControl::stopPlayback()
{
    if (m_engine)
        m_engine->stop();
}

void PlaybackControl::setPlaybackSpeed(double speed)
{
    if (m_engine)
        m_engine->setPlaybackSpeed(speed);
}

void PlaybackControl::seekToTime(const QDateTime& time)
{
    if (m_engine)
        m_engine->seekToTime(time);
}

void PlaybackControl::seekToProgress(double progress)
{
    if (m_engine)
        m_engine->seekToProgress(progress);
}

void PlaybackControl::setDraggingMode(bool isDragging)
{
    if (m_engine)
        m_engine->setDraggingMode(isDragging);
}

bool PlaybackControl::hasValidPlaybackTimeRange() const
{
    const QDateTime st = startTime();
    const QDateTime en = endTime();
    return st.isValid() && en.isValid() && st < en;
}

bool PlaybackControl::playbackIsLongTerm() const
{
    if (!hasValidPlaybackTimeRange())
        return false;
    const qint64 ms = startTime().msecsTo(endTime());
    return ms > qint64(7) * 24 * 3600 * 1000;
}

bool PlaybackControl::playbackSpansMultipleDays() const
{
    if (!hasValidPlaybackTimeRange())
        return false;
    return startTime().date() != endTime().date();
}

QStringList PlaybackControl::playbackSpeedLabels() const
{
    if (playbackIsLongTerm())
        return {QStringLiteral("0.1x"), QStringLiteral("0.5x"), QStringLiteral("1x"), QStringLiteral("2x"),
                QStringLiteral("5x"), QStringLiteral("10x"), QStringLiteral("50x"), QStringLiteral("100x")};
    return {QStringLiteral("0.5x"), QStringLiteral("1x"), QStringLiteral("2x"), QStringLiteral("5x"), QStringLiteral("10x")};
}

int PlaybackControl::playbackSpeedDefaultIndex() const
{
    const QStringList labels = playbackSpeedLabels();
    const int idx = labels.indexOf(QStringLiteral("1x"));
    return idx >= 0 ? idx : 0;
}

QString PlaybackControl::playbackTimeRangeSummary() const
{
    if (!hasValidPlaybackTimeRange())
        return {};
    const QDateTime st = startTime();
    const QDateTime en = endTime();
    const qint64 totalMs = st.msecsTo(en);
    if (totalMs <= 0)
        return {};
    const qint64 totalDays = totalMs / (1000LL * 60 * 60 * 24);
    const qint64 totalHours = totalMs / (1000LL * 60 * 60);
    if (totalDays > 365) {
        const qint64 years = totalDays / 365;
        const qint64 rem = totalDays % 365;
        return QStringLiteral("%1年%2天").arg(years).arg(rem);
    }
    if (totalDays > 30) {
        const qint64 months = totalDays / 30;
        const qint64 rem = totalDays % 30;
        return QStringLiteral("%1月%2天").arg(months).arg(rem);
    }
    if (totalDays > 0)
        return QStringLiteral("%1天").arg(totalDays);
    return QStringLiteral("%1小时").arg(totalHours);
}

void PlaybackControl::seekProgressDelta(double delta)
{
    const double p = qBound(0.0, m_playbackProgress + delta, 1.0);
    seekToProgress(p);
}

double PlaybackControl::playbackSpeedMultiplierAtIndex(int index) const
{
    const QStringList labels = playbackSpeedLabels();
    if (index < 0 || index >= labels.size())
        return 1.0;
    QString s = labels.at(index);
    s.remove(QLatin1Char('x'));
    bool ok = false;
    const double v = s.toDouble(&ok);
    return ok && v > 0 ? v : 1.0;
}

void PlaybackControl::setPlaybackSpeedFromLabelIndex(int index)
{
    setPlaybackSpeed(playbackSpeedMultiplierAtIndex(index));
}

QString PlaybackControl::formatSeekTooltip(double progress) const
{
    if (!hasValidPlaybackTimeRange())
        return {};
    const QDateTime t = progressToTime(progress);
    if (!t.isValid())
        return {};
    const int pct = qRound(qBound(0.0, progress, 1.0) * 100.0);
    if (playbackSpansMultipleDays())
        return QStringLiteral("%1\n进度: %2%").arg(t.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))).arg(pct);
    return QStringLiteral("%1\n进度: %2%").arg(t.toString(QStringLiteral("hh:mm:ss"))).arg(pct);
}

QString PlaybackControl::playbackTimeRangeTooltip() const
{
    if (!hasValidPlaybackTimeRange())
        return {};
    const QDateTime st = startTime();
    const QDateTime en = endTime();
    return QStringLiteral("数据时间跨度: %1\n开始: %2\n结束: %3")
        .arg(playbackTimeRangeSummary())
        .arg(st.toString(QStringLiteral("yyyy-MM-dd hh:mm")))
        .arg(en.toString(QStringLiteral("yyyy-MM-dd hh:mm")));
}

QDateTime PlaybackControl::progressToTime(double progress) const
{
    const QDateTime st = startTime();
    const QDateTime en = endTime();
    if (!st.isValid() || !en.isValid())
        return QDateTime();

    progress = qBound(0.0, progress, 1.0);
    const qint64 totalMs = st.msecsTo(en);
    const qint64 targetMs = static_cast<qint64>(totalMs * progress);
    return st.addMSecs(targetMs);
}

double PlaybackControl::timeToProgress(const QDateTime& time) const
{
    const QDateTime st = startTime();
    const QDateTime en = endTime();
    if (!st.isValid() || !en.isValid() || !time.isValid())
        return 0.0;

    const qint64 totalMs = st.msecsTo(en);
    if (totalMs <= 0)
        return 0.0;

    const qint64 currentMs = st.msecsTo(time);
    return qBound(0.0, static_cast<double>(currentMs) / totalMs, 1.0);
}
