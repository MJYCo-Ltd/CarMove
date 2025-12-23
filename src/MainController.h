#ifndef MAINCONTROLLER_H
#define MAINCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QVariantList>
#include <QQmlEngine>
#include <QGeoCoordinate>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QPixmap>
#include <QStandardPaths>
#include <QDir>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QWindow>
#include <QSet>
#include <QDate>
#include "FolderScanner.h"
#include "ExcelDataReader.h"
#include "VehicleAnimationEngine.h"

class VehicleManager;
class VehicleAnimationEngine;
class VehicleDataModel;
class CoordinateConverter;

class MainController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    
    Q_PROPERTY(QString currentFolder READ currentFolder NOTIFY currentFolderChanged)
    Q_PROPERTY(QStringList vehicleList READ vehicleList NOTIFY vehicleListChanged)
    Q_PROPERTY(QString selectedVehicle READ selectedVehicle NOTIFY selectedVehicleChanged)
    Q_PROPERTY(QDateTime startTime READ startTime NOTIFY timeRangeChanged)
    Q_PROPERTY(QDateTime endTime READ endTime NOTIFY timeRangeChanged)
    Q_PROPERTY(QDateTime currentTime READ currentTime NOTIFY currentTimeChanged)
    Q_PROPERTY(bool coordinateConversionEnabled READ coordinateConversionEnabled WRITE setCoordinateConversionEnabled NOTIFY coordinateConversionChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playbackStateChanged)
    Q_PROPERTY(double playbackProgress READ playbackProgress NOTIFY progressChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(QString loadingMessage READ loadingMessage NOTIFY loadingMessageChanged)
    
public:
    explicit MainController(QObject *parent = nullptr);
    ~MainController();
    
    // Property getters
    QString currentFolder() const { return m_currentFolder; }
    QStringList vehicleList() const { return m_vehicleList; }
    QString selectedVehicle() const { return m_selectedVehicle; }
    QDateTime startTime() const { return m_startTime; }
    QDateTime endTime() const { return m_endTime; }
    QDateTime currentTime() const { return m_currentTime; }
    bool coordinateConversionEnabled() const { return m_coordinateConversionEnabled; }
    bool isPlaying() const { return m_isPlaying; }
    double playbackProgress() const { return m_playbackProgress; }
    bool isLoading() const { return m_isLoading; }
    QString loadingMessage() const { return m_loadingMessage; }
    
    // Property setters
    void setCoordinateConversionEnabled(bool enabled);
    
    // Invokable methods for QML
    Q_INVOKABLE void selectFolder(const QString& folderPath);
    Q_INVOKABLE void selectVehicle(const QString& plateNumber);
    Q_INVOKABLE void toggleCoordinateConversion();
    Q_INVOKABLE QVariantList getConvertedTrajectory();
    Q_INVOKABLE QVariantList getCurrentTrajectory();
    Q_INVOKABLE void startPlayback();
    Q_INVOKABLE void pausePlayback();
    Q_INVOKABLE void stopPlayback();
    Q_INVOKABLE void setPlaybackSpeed(double speed);
    Q_INVOKABLE void seekToTime(const QDateTime& time);
    Q_INVOKABLE void seekToProgress(double progress);
    Q_INVOKABLE QString getVehicleInfo(const QString& plateNumber);
    Q_INVOKABLE void refreshVehicleList();
    Q_INVOKABLE QDateTime progressToTime(double progress);
    Q_INVOKABLE double timeToProgress(const QDateTime& time);
    Q_INVOKABLE void setDraggingMode(bool isDragging);
    Q_INVOKABLE void takeMapScreenshot(const QString& fileName);
    Q_INVOKABLE int calculateVisitDays(const QString& plateNumber, double targetLat, double targetLon, double radiusMeters);
    Q_INVOKABLE QString getDocumentsPath();
    
signals:
    void folderScanned(bool success, const QString& message);
    void vehicleListChanged();
    void selectedVehicleChanged();
    void trajectoryLoaded(bool success, const QString& message);
    void trajectoryConverted();
    void currentFolderChanged();
    void timeRangeChanged();
    void currentTimeChanged();
    void coordinateConversionChanged();
    void playbackStateChanged();
    void progressChanged();
    void vehiclePositionUpdated(const QString& plateNumber, 
                               const QGeoCoordinate& position, 
                               int direction, double speed);
    void errorOccurred(const QString& error);
    void loadingProgress(int percentage);
    void loadingChanged();
    void loadingMessageChanged();
    
private slots:
    void onFolderScanCompleted(const QList<FolderScanner::VehicleInfo>& vehicles);
    void onFolderScanError(const QString& error);
    void onFolderScanProgress(int percentage);
    void onVehicleTrajectoryLoaded(const QString& plateNumber, 
                                  const QList<ExcelDataReader::VehicleRecord>& trajectory);
    void onTrajectoryConverted(const QString& plateNumber,
                              const QList<ExcelDataReader::VehicleRecord>& convertedTrajectory);
    void onVehicleLoadingProgress(int percentage);
    void onAnimationCurrentTimeChanged(const QDateTime& time);
    void onAnimationProgressChanged(double progress);
    void onAnimationPlaybackStateChanged(VehicleAnimationEngine::PlaybackState state);
    void onVehiclePositionUpdate(const QString& plateNumber, 
                                const QGeoCoordinate& position, 
                                int direction, double speed);
    
private:
    // Helper methods
    void updateTimeRange();
    void setupVehicleDataModel();
    QVariantMap vehicleRecordToVariant(const ExcelDataReader::VehicleRecord& record);
    
    // Properties
    QString m_currentFolder;
    QStringList m_vehicleList;
    QString m_selectedVehicle;
    QDateTime m_startTime;
    QDateTime m_endTime;
    QDateTime m_currentTime;
    bool m_coordinateConversionEnabled;
    bool m_isPlaying;
    double m_playbackProgress;
    bool m_isLoading;
    QString m_loadingMessage;
    
    // Component instances
    FolderScanner* m_folderScanner;
    VehicleManager* m_vehicleManager;
    VehicleAnimationEngine* m_animationEngine;
    VehicleDataModel* m_vehicleDataModel;
    
    // Current vehicle info cache
    QList<FolderScanner::VehicleInfo> m_vehicleInfoList;
};

#endif // MAINCONTROLLER_H