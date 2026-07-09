#ifndef PLAYBACKCONTROL_H
#define PLAYBACKCONTROL_H

#include <QObject>
#include <QDateTime>
#include <QStringList>
#include <QQmlEngine>

#include "VehicleAnimationEngine.h"

class VehicleDataModel;

/// 轨迹推演与播放 UI 状态（定时器在 VehicleAnimationEngine 内；本类负责对外属性与辅助计算）
class PlaybackControl : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playbackStateChanged)
    Q_PROPERTY(double playbackProgress READ playbackProgress NOTIFY progressChanged)
    Q_PROPERTY(QDateTime startTime READ startTime NOTIFY timeRangeChanged)
    Q_PROPERTY(QDateTime endTime READ endTime NOTIFY timeRangeChanged)
    Q_PROPERTY(QDateTime currentTime READ currentTime NOTIFY currentTimeChanged)
    Q_PROPERTY(bool hasValidPlaybackTimeRange READ hasValidPlaybackTimeRange NOTIFY timeRangeChanged)
    Q_PROPERTY(bool playbackIsLongTerm READ playbackIsLongTerm NOTIFY timeRangeChanged)
    Q_PROPERTY(bool playbackSpansMultipleDays READ playbackSpansMultipleDays NOTIFY timeRangeChanged)
    Q_PROPERTY(QStringList playbackSpeedLabels READ playbackSpeedLabels NOTIFY timeRangeChanged)
    Q_PROPERTY(int playbackSpeedDefaultIndex READ playbackSpeedDefaultIndex NOTIFY timeRangeChanged)
    Q_PROPERTY(QString playbackTimeRangeSummary READ playbackTimeRangeSummary NOTIFY timeRangeChanged)

public:
    explicit PlaybackControl(VehicleAnimationEngine* engine, VehicleDataModel* model, QObject* parent = nullptr);

    bool isPlaying() const { return m_isPlaying; }
    double playbackProgress() const { return m_playbackProgress; }
    QDateTime startTime() const;
    QDateTime endTime() const;
    QDateTime currentTime() const { return m_currentTime; }
    bool hasValidPlaybackTimeRange() const;
    bool playbackIsLongTerm() const;
    bool playbackSpansMultipleDays() const;
    QStringList playbackSpeedLabels() const;
    int playbackSpeedDefaultIndex() const;
    QString playbackTimeRangeSummary() const;

    Q_INVOKABLE void startPlayback();
    Q_INVOKABLE void pausePlayback();
    Q_INVOKABLE void stopPlayback();
    Q_INVOKABLE void setPlaybackSpeed(double speed);
    Q_INVOKABLE void seekToTime(const QDateTime& time);
    Q_INVOKABLE void seekToProgress(double progress);
    Q_INVOKABLE void setDraggingMode(bool isDragging);
    Q_INVOKABLE void seekProgressDelta(double delta);
    Q_INVOKABLE void setPlaybackSpeedFromLabelIndex(int index);
    Q_INVOKABLE double playbackSpeedMultiplierAtIndex(int index) const;
    Q_INVOKABLE QDateTime progressToTime(double progress) const;
    Q_INVOKABLE double timeToProgress(const QDateTime& time) const;
    Q_INVOKABLE QString formatSeekTooltip(double progress) const;
    Q_INVOKABLE QString playbackTimeRangeTooltip() const;

    /// 由 MainController 在轨迹/模型时间范围变化后调用
    void notifyModelTimeRangeUpdated();

signals:
    void playbackStateChanged();
    void progressChanged();
    void currentTimeChanged();
    void timeRangeChanged();

private slots:
    void onEngineCurrentTimeChanged(const QDateTime& time);
    void onEngineProgressChanged(double progress);
    void onEnginePlaybackStateChanged(VehicleAnimationEngine::PlaybackState state);

private:
    VehicleAnimationEngine* m_engine = nullptr;
    VehicleDataModel* m_model = nullptr;
    bool m_isPlaying = false;
    double m_playbackProgress = 0.0;
    QDateTime m_currentTime;
    QDateTime m_lastNotifiedRangeStart;
    QDateTime m_lastNotifiedRangeEnd;
};

#endif
