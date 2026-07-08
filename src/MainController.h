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
#include "FolderScanner.h"
#include "ParseData/ExcelDataReader.h"
#include "VehicleAnimationEngine.h"
#include "PlaybackControl.h"
#include "ConfigManager.h"
#include "PostGisTrajectoryLoader.h"

class VehicleManager;
class VehicleDataModel;

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
    Q_PROPERTY(PlaybackControl* playback READ playback CONSTANT)
    Q_PROPERTY(QString trajectorySourceMode READ trajectorySourceMode NOTIFY trajectorySourceModeChanged)
    Q_PROPERTY(bool useDatabaseTrajectorySource READ useDatabaseTrajectorySource NOTIFY trajectorySourceModeChanged)
    Q_PROPERTY(bool databaseConnected READ databaseConnected NOTIFY databaseConnectionChanged)
    Q_PROPERTY(QString databaseStatus READ databaseStatus NOTIFY databaseConnectionChanged)
    /// 目标区域中心（「定位到目标区域」、统计经过次数、搜索「设为目标区域」共用）
    Q_PROPERTY(double targetAreaLatitude READ targetAreaLatitude WRITE setTargetAreaLatitude NOTIFY targetAreaChanged)
    Q_PROPERTY(double targetAreaLongitude READ targetAreaLongitude WRITE setTargetAreaLongitude NOTIFY targetAreaChanged)
    /// 目标区域名称（搜索/右键 POI 写入；为空时由地图侧逆地理补全）
    Q_PROPERTY(QString targetAreaName READ targetAreaName WRITE setTargetAreaName NOTIFY targetAreaChanged)

public:
    explicit MainController(QObject *parent = nullptr);
    ~MainController();

    QString currentFolder() const { return m_currentFolder; }
    QStringList vehicleList() const { return m_vehicleList; }
    QStringList filteredVehicleList() const { return m_filteredVehicleList; }
    QString searchText() const { return m_searchText; }
    QString selectedVehicle() const { return m_selectedVehicle; }
    bool coordinateConversionEnabled() const { return m_coordinateConversionEnabled; }
    bool isLoading() const { return m_isLoading; }
    QString loadingMessage() const { return m_loadingMessage; }
    ConfigManager* configManager() const { return ConfigManager::GetInstance(); }
    PlaybackControl* playback() const { return m_playbackControl; }
    QString trajectorySourceMode() const;
    bool useDatabaseTrajectorySource() const;
    bool databaseConnected() const { return m_databaseConnected; }
    QString databaseStatus() const { return m_databaseStatus; }
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
    Q_INVOKABLE void toggleCoordinateConversion();
    Q_INVOKABLE QVariantList getConvertedTrajectory();
    /// 一次设置目标区域中心与名称（name 可为空；空名称时由 QML 逆地理补全）
    Q_INVOKABLE void setTargetAreaCenter(double latitude, double longitude, const QString& name);
    /// 当前选中车辆轨迹上，进入目标半径区域的次数（由「区外→区内」跳变计一次）
    Q_INVOKABLE int calculateTargetAreaVisitCount(const QString& plateNumber, double targetLat, double targetLon, double radiusMeters) const;
    /// 将轨迹点 QVariant（含 coordinate 或 latitude/longitude）转为 QGeoCoordinate
    Q_INVOKABLE QGeoCoordinate trajectoryPointToCoordinate(const QVariant& point) const;
    /// 车牌字符串哈希配色（与地图车辆色一致）
    Q_INVOKABLE QString colorHexForPlate(const QString& plateNumber) const;
    /// 轨迹点序列 → MapPolyline.path 可用的坐标列表
    Q_INVOKABLE QVariantList trajectoryPolylinePath(const QVariantList& trajectoryPoints) const;
    /// 轨迹点序列 → QGeoPath（用于 fitViewportToGeoShape）
    Q_INVOKABLE QVariant geoPathFromTrajectory(const QVariantList& trajectoryPoints) const;
    /// 卸油记录列表 amount 字段求和，格式化为两位小数（吨）
    Q_INVOKABLE QString formatRecordsTotalAmount(const QVariantList& records) const;
    /// 批量计算目标区域经过次数（车牌列表由 QML 传入）
    Q_INVOKABLE QVariantMap batchTargetAreaVisitCounts(const QVariantList& plateNumbers, double lat, double lon, double radiusMeters) const;
    /// 车辆位置是否变化小于阈值（米），用于跳过无意义刷新
    Q_INVOKABLE bool isVehicleMoveBelowDistanceThreshold(const QGeoCoordinate& prevCoord, const QGeoCoordinate& newCoord, double thresholdMeters) const;
    /// 定位到指定地点时，将当前车辆回放位置跳到距该点最近的轨迹点
    Q_INVOKABLE bool seekVehicleToNearestTrajectoryPoint(double latitude, double longitude);
    Q_INVOKABLE QString getDocumentsPath();
    Q_INVOKABLE void clearSearch();

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
    void loadingProgress(int percentage);
    void loadingChanged();
    void loadingMessageChanged();
    void targetAreaChanged();

private slots:
    void onFolderScanCompleted(const QList<FolderScanner::VehicleInfo>& vehicles);
    void onFolderScanError(const QString& error);
    void onFolderScanProgress(int percentage);
    void onVehicleTrajectoryLoaded(const QString& plateNumber,
                                  const QList<ExcelDataReader::VehicleRecord>& trajectory);
    void onTrajectoryConverted(const QString& plateNumber,
                              const QList<ExcelDataReader::VehicleRecord>& convertedTrajectory);
    void onVehicleLoadingProgress(int percentage);
    void onVehiclePositionUpdate(const QString& plateNumber,
                                const QGeoCoordinate& position,
                                int direction, double speed);

private:
    void updateTimeRange();
    void setupVehicleDataModel();
    QVariantMap vehicleRecordToVariant(const ExcelDataReader::VehicleRecord& record);
    void updateFilteredVehicleList();
    void persistTargetAreaConfig();
    void applyTrajectorySourceMode();
    void clearVehicleDataState();

    QString m_currentFolder;
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

    FolderScanner* m_folderScanner;
    PostGisTrajectoryLoader* m_postGisLoader;
    VehicleManager* m_vehicleManager;
    VehicleAnimationEngine* m_animationEngine;
    VehicleDataModel* m_vehicleDataModel;
    PlaybackControl* m_playbackControl = nullptr;
    bool m_databaseConnected = false;
    QString m_databaseStatus;

    QList<FolderScanner::VehicleInfo> m_vehicleInfoList;
};

#endif // MAINCONTROLLER_H
