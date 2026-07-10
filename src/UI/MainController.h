#ifndef MAINCONTROLLER_H
#define MAINCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QVariantList>
#include <QVariantMap>
#include <QVariant>
#include <QGeoCoordinate>
#include "Domain/TrajectoryTypes.h"
#include "Core/ConfigManager.h"
#include "DataManagement/TrajectoryDataManager.h"
#include "DataManagement/TrajectoryTimelineManager.h"
#include "Map/MapServiceManager.h"
#include "Core/FilePathManager.h"

class VehicleManager;
class MainController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString currentFolder READ currentFolder NOTIFY currentFolderChanged)
    Q_PROPERTY(QStringList vehicleList READ vehicleList NOTIFY vehicleListChanged)
    Q_PROPERTY(QStringList filteredVehicleList READ filteredVehicleList NOTIFY filteredVehicleListChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(QString selectedVehicle READ selectedVehicle NOTIFY selectedVehicleChanged)
    Q_PROPERTY(bool coordinateConversionEnabled READ coordinateConversionEnabled WRITE setCoordinateConversionEnabled NOTIFY coordinateConversionChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(QString loadingMessage READ loadingMessage NOTIFY loadingMessageChanged)
    Q_PROPERTY(ConfigManager* configManager READ configManager CONSTANT)
    Q_PROPERTY(QDateTime trajectoryStartTime READ trajectoryStartTime NOTIFY trajectoryTimeRangeChanged)
    Q_PROPERTY(QDateTime trajectoryEndTime READ trajectoryEndTime NOTIFY trajectoryTimeRangeChanged)
    Q_PROPERTY(QDateTime trajectoryCurrentTime READ trajectoryCurrentTime NOTIFY trajectoryCurrentTimeChanged)
    Q_PROPERTY(bool trajectorySpansMultipleDays READ trajectorySpansMultipleDays NOTIFY trajectoryTimeRangeChanged)
    Q_PROPERTY(TrajectoryDataManager* trajectoryData READ trajectoryData CONSTANT)
    Q_PROPERTY(MapServiceManager* mapService READ mapService CONSTANT)
    Q_PROPERTY(FilePathManager* paths READ paths CONSTANT)
    Q_PROPERTY(QString trajectorySourceMode READ trajectorySourceMode NOTIFY trajectorySourceModeChanged)
    Q_PROPERTY(bool useDatabaseTrajectorySource READ useDatabaseTrajectorySource NOTIFY trajectorySourceModeChanged)
    Q_PROPERTY(bool databaseConnected READ databaseConnected NOTIFY databaseConnectionChanged)
    Q_PROPERTY(QString databaseStatus READ databaseStatus NOTIFY databaseConnectionChanged)
    Q_PROPERTY(double targetAreaLatitude READ targetAreaLatitude WRITE setTargetAreaLatitude NOTIFY targetAreaChanged)
    Q_PROPERTY(double targetAreaLongitude READ targetAreaLongitude WRITE setTargetAreaLongitude NOTIFY targetAreaChanged)
    Q_PROPERTY(QString targetAreaName READ targetAreaName WRITE setTargetAreaName NOTIFY targetAreaChanged)

public:
    explicit MainController(QObject *parent = nullptr);
    ~MainController();

    QString currentFolder() const;
    QStringList vehicleList() const { return m_vehicleList; }
    QStringList filteredVehicleList() const { return m_filteredVehicleList; }
    QString searchText() const { return m_searchText; }
    QString selectedVehicle() const { return m_selectedVehicle; }
    bool coordinateConversionEnabled() const { return m_coordinateConversionEnabled; }
    bool isLoading() const { return m_isLoading; }
    QString loadingMessage() const { return m_loadingMessage; }
    ConfigManager* configManager() const { return ConfigManager::GetInstance(); }
    QDateTime trajectoryStartTime() const;
    QDateTime trajectoryEndTime() const;
    QDateTime trajectoryCurrentTime() const;
    bool trajectorySpansMultipleDays() const;
    TrajectoryDataManager* trajectoryData() const { return m_trajectoryDataManager; }
    MapServiceManager* mapService() const { return m_mapServiceManager; }
    FilePathManager* paths() const { return m_filePathManager; }
    QString trajectorySourceMode() const;
    bool useDatabaseTrajectorySource() const;
    bool databaseConnected() const;
    QString databaseStatus() const;
    double targetAreaLatitude() const { return m_targetAreaLatitude; }
    double targetAreaLongitude() const { return m_targetAreaLongitude; }
    QString targetAreaName() const { return m_targetAreaName; }

    void setTargetAreaLatitude(double lat);
    void setTargetAreaLongitude(double lon);
    void setTargetAreaName(const QString& name);
    void setCoordinateConversionEnabled(bool enabled);
    Q_INVOKABLE void setSearchText(const QString& text);

    Q_INVOKABLE void selectFolder(const QString& folderPath);
    Q_INVOKABLE void connectPostGisDatabase();
    Q_INVOKABLE void setTrajectorySourceMode(const QString& mode);
    Q_INVOKABLE void savePostGisSettings();
    Q_INVOKABLE void selectVehicle(const QString& plateNumber);
    Q_INVOKABLE void loadTrajectoryForCapture(const QString& plateNumber,
                                              const QString& startDateIso,
                                              const QString& endDateIso);
    Q_INVOKABLE QString normalizeLocalPath(const QString& path) const;
    Q_INVOKABLE bool ensureScreenshotOutputDirectory(const QString& folderPath) const;
    Q_INVOKABLE QString screenshotFilePath(const QString& folderPath,
                                             const QString& plateNumber,
                                             const QString& startDateIso,
                                             const QString& endDateIso) const;
    Q_INVOKABLE QString targetAreaScreenshotFilePath(const QString& folderPath,
                                                     const QString& plateNumber,
                                                     const QString& startDateIso,
                                                     const QString& endDateIso) const;
    Q_INVOKABLE bool screenshotFileExists(const QString& folderPath,
                                            const QString& plateNumber,
                                            const QString& startDateIso,
                                            const QString& endDateIso) const;
    Q_INVOKABLE void toggleCoordinateConversion();
    Q_INVOKABLE QVariantList getConvertedTrajectory();
    Q_INVOKABLE void setTargetAreaCenter(double latitude, double longitude, const QString& name);
    Q_INVOKABLE int calculateTargetAreaVisitCount(const QString& plateNumber, double targetLat, double targetLon, double radiusMeters) const;
    Q_INVOKABLE int targetAreaVisitCountForPlate(const QString& plateNumber) const;
    Q_INVOKABLE QGeoCoordinate trajectoryPointToCoordinate(const QVariant& point) const;
    Q_INVOKABLE QGeoCoordinate targetAreaMapCoordinate() const;
    Q_INVOKABLE QString colorHexForPlate(const QString& plateNumber) const;
    Q_INVOKABLE QVariantList trajectoryPolylinePath(const QVariant& trajectoryPoints) const;
    Q_INVOKABLE QVariantList trajectoryPointSegments(const QVariant& trajectoryPoints) const;
    Q_INVOKABLE QVariantList trajectorySegmentPolylinePaths(const QVariant& trajectoryPoints) const;
    Q_INVOKABLE int trajectoryDisplaySegmentCount();
    Q_INVOKABLE QVariantList trajectoryDisplaySegmentPath(int segmentIndex) const;
    Q_INVOKABLE void seekTrajectoryToProgress(double progress);
    Q_INVOKABLE int trajectorySegmentCount() const;
    Q_INVOKABLE QDateTime trajectorySegmentStartTime(int segmentIndex) const;
    Q_INVOKABLE QDateTime trajectorySegmentEndTime(int segmentIndex) const;
    Q_INVOKABLE int trajectoryActiveSegmentIndex() const;
    Q_INVOKABLE double trajectorySegmentLocalProgress(int segmentIndex) const;
    Q_INVOKABLE void seekTrajectorySegment(int segmentIndex, double localProgress);
    Q_INVOKABLE QVariant geoPathForViewport(const QVariant& trajectoryPoints) const;
    Q_INVOKABLE QString formatRecordsTotalAmount(const QVariantList& records) const;
    Q_INVOKABLE QVariantMap batchTargetAreaVisitCounts(const QVariantList& plateNumbers, double lat, double lon, double radiusMeters) const;
    Q_INVOKABLE bool isVehicleMoveBelowDistanceThreshold(const QGeoCoordinate& prevCoord, const QGeoCoordinate& newCoord, double thresholdMeters) const;
    Q_INVOKABLE bool seekVehicleToNearestTrajectoryPoint(double latitude, double longitude);
    Q_INVOKABLE QString getDocumentsPath();
    Q_INVOKABLE void clearSearch();

    void prepareForApplicationShutdown();

signals:
    void folderScanned(bool success, const QString& message);
    void vehicleListChanged();
    void filteredVehicleListChanged();
    void searchTextChanged();
    void selectedVehicleChanged();
    void trajectorySourceModeChanged();
    void databaseConnectionChanged();
    void trajectoryLoaded(bool success, const QString& message);
    void trajectoryConverted();
    void currentFolderChanged();
    void coordinateConversionChanged();
    void vehiclePositionUpdated(const QString& plateNumber,
                               const QGeoCoordinate& position,
                               int direction, double speed);
    void errorOccurred(const QString& error);
    void captureTrajectoryReady(bool success, int pointCount);
    void loadingProgress(int percentage);
    void loadingChanged();
    void loadingMessageChanged();
    void targetAreaChanged();
    void trajectoryTimeRangeChanged();
    void trajectoryCurrentTimeChanged();
    void trajectorySegmentsChanged();

private slots:
    void onDataSourceScanCompleted(const QList<TrajectoryDataManager::VehicleInfo>& vehicles);
    void onDataSourceScanError(const QString& error);
    void onDataSourceScanProgress(int percentage);
    void onDataSourceReadyChanged();
    void onVehicleTrajectoryLoaded(const QString& plateNumber,
                                  const QList<TrajectoryPoint>& trajectory);
    void onTrajectoryConverted(const QString& plateNumber,
                              const QList<TrajectoryPoint>& convertedTrajectory);
    void onVehicleLoadingProgress(int percentage);

private:
    void syncTimelineFromVehicleManager();
    void resetTrajectoryTimeline();
    QVariantMap vehicleRecordToVariant(const TrajectoryPoint& record) const;
    void updateFilteredVehicleList();
    void persistTargetAreaConfig();
    void clearVehicleDataState();
    void finishVehicleListLoad(const QList<TrajectoryDataManager::VehicleInfo>& vehicles);
    void updateDatabaseConnectionState();

    QStringList m_vehicleList;
    QStringList m_filteredVehicleList;
    QString m_searchText;
    QString m_selectedVehicle;
    bool m_coordinateConversionEnabled;
    bool m_isLoading;
    QString m_loadingMessage;
    double m_targetAreaLatitude = 38.97887422901859;
    double m_targetAreaLongitude = 117.73397758792544;
    QString m_targetAreaName;

    TrajectoryDataManager* m_trajectoryDataManager;
    MapServiceManager* m_mapServiceManager;
    FilePathManager* m_filePathManager;
    VehicleManager* m_vehicleManager;
    TrajectoryTimelineManager* m_timelineManager = nullptr;
    QString m_databaseStatus;
    bool m_captureTrajectoryPending = false;

    QList<TrajectoryDataManager::VehicleInfo> m_vehicleInfoList;
};

#endif // MAINCONTROLLER_H
